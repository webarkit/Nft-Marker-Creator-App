#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <AR/ar.h>
#include <AR2/config.h>
#include <AR2/featureSet.h>
#include <AR2/imageSet.h>

static std::mutex g_mapLock;

static int get_similarity( ARUint8 *imageBW, int xsize, int ysize,
    float  *tmpl, float  vlen, int ts1, int ts2,
    int cx, int cy, float  *sim);

    static int get_similarity( ARUint8 *imageBW, int xsize, int ysize,
        float *tmpl, float vlen, int ts1, int ts2,
        int cx, int cy, float  *sim)
{
#if 0
ARUint8   *ip;
float     *tp;
float     ave2, w1, w2, vlen2;
int       i, j;

if( cy - ts1 < 0 || cy + ts2 >= ysize || cx - ts1 < 0 || cx + ts2 >= xsize ) return -1;

ave2 = 0.0f;
for( j = -ts1; j <= ts2; j++ ) {
ip = &imageBW[(cy+j)*xsize+(cx-ts1)];
for( i = -ts1; i <= ts2 ; i++ ) ave2 += *(ip++);
}
ave2 /= (ts1+ts2+1)*(ts1+ts2+1);

tp = tmpl;
w1 = 0.0f;
vlen2 = 0.0f;
for( j = -ts1; j <= ts2; j++ ) {
ip = &imageBW[(cy+j)*xsize+(cx-ts1)];
for( i = -ts1; i <= ts2 ; i++ ) {
w2 = (float )(*(ip++)) - ave2;
vlen2 += w2 * w2;
w1 += *(tp++) * w2;
}
}
if( vlen2 == 0.0f ) return -1;

vlen2 = sqrtf(vlen2);
*sim = w1 / (vlen * vlen2);
#else
ARUint8   *ip;
float     *tp;
float     sx, sxx, sxy;
float     vlen2;
int       i, j;

if( cy - ts1 < 0 || cy + ts2 >= ysize || cx - ts1 < 0 || cx + ts2 >= xsize ) return -1;

tp = tmpl;
sx = sxx = sxy = 0.0f;
for( j = -ts1; j <= ts2; j++ ) {
ip = &imageBW[(cy+j)*xsize+(cx-ts1)];
for( i = -ts1; i <= ts2 ; i++ ) {
sx += *ip;
sxx += *ip * *ip;
sxy += *(ip++) * *(tp++);
}
}
vlen2 = sxx - sx*sx/((ts1+ts2+1)*(ts1+ts2+1));
if( vlen2 == 0.0f ) return -1;
vlen2 = sqrtf(vlen2);

*sim = sxy / (vlen * vlen2);
#endif

return 0;
}

static int make_template( ARUint8 *imageBW, int xsize, int ysize,
    int cx, int cy, int ts1, int ts2, float  sd_thresh,
    float *tmpl, float *vlen )
{
ARUint8  *ip;
float    *tp;
float     vlen1, ave;
int       i, j;

if( cy - ts1 < 0 || cy + ts2 >= ysize || cx - ts1 < 0 || cx + ts2 >= xsize ) return -1;

ave = 0.0f;
for( j = -ts1; j <= ts2; j++ ) {
ip = &imageBW[(cy+j)*xsize+(cx-ts1)];
for( i = -ts1; i <= ts2 ; i++ ) ave += *(ip++);
}
ave /= (ts1+ts2+1)*(ts1+ts2+1);

tp = tmpl;
vlen1 = 0.0f;
for( j = -ts1; j <= ts2; j++ ) {
ip = &imageBW[(cy+j)*xsize+(cx-ts1)];
for( i = -ts1; i <= ts2 ; i++ ) {
*tp = (float )(*(ip++)) - ave;
vlen1 += *tp * *tp;
tp++;
}
}

if( vlen1 == 0.0f ) return -1;
if( vlen1/((ts1+ts2+1)*(ts1+ts2+1)) < sd_thresh*sd_thresh ) return -1;

*vlen = sqrtf(vlen1);

return 0;
}

AR2FeatureMapT *ar2GenFeatureMapThreaded(AR2ImageT *image,
                                         int ts1, int ts2,
                                         int search_size1, int search_size2,
                                         float max_sim_thresh, float sd_thresh, int threadCount)
{
    AR2FeatureMapT  *featureMap;
    int xsize = image->xsize;
    int ysize = image->ysize;
    float *fimage = nullptr; // shared buffer for final results
    float *fp = nullptr;     // pointer to current position in fimage
    float *fimage2 = nullptr; // shared buffer for intermediate results
    float *fp2 = nullptr; // pointer to current position in fimage2
    float *tmpl = nullptr; // shared buffer for template
    ARUint8 *p = nullptr; // pointer to current position in image->imgBW
    float dx, dy;
    int hist[1000], sum;
    int i, j, k;
    float vlen;
    float max, sim;
    int ii, jj;

    arMalloc(fimage,   float,  xsize*ysize);
    arMalloc(fimage2,  float,  xsize*ysize);
    arMalloc(tmpl, float , (ts1+ts2+1)*(ts1+ts2+1));

    // Allocate the final featureMap object
    fp2 = fimage2;
    #if AR2_CAPABLE_ADAPTIVE_TEMPLATE
        p = image->imgBWBlur[1];
    #else
        p = image->imgBW;
    #endif

    // Initialize fimage2 with edge detection
    auto initEdges = [&](int startRow, int endRow) {
        for (int j = startRow; j < endRow; j++) {
            for (int i = 0; i < xsize; i++) {
                if (j == 0 || j == ysize - 1 || i == 0 || i == xsize - 1) {
                    fimage2[j * xsize + i] = -1.0f;
                } else {
                    dx = ((int)(*(p - xsize + 1)) - (int)(*(p - xsize - 1))
                        + (int)(*(p + 1)) - (int)(*(p - 1))
                        + (int)(*(p + xsize + 1)) - (int)(*(p + xsize - 1))) / (float)(3.0f * 256);
                    dy = ((int)(*(p + xsize + 1)) - (int)(*(p - xsize + 1))
                        + (int)(*(p + xsize)) - (int)(*(p - xsize))
                        + (int)(*(p + xsize - 1)) - (int)(*(p - xsize - 1))) / (float)(3.0f * 256);
                    fimage2[j * xsize + i] = (float)sqrtf((dx * dx + dy * dy) / (float)2.0f);
                }
                p++;
            }
        }
    };

    std::vector<std::thread> threads;
    int rowsPerThread = ysize / threadCount;
    int currentStart = 0;

    for (int t = 0; t < threadCount; t++) {
        int currentEnd = (t == threadCount - 1) ? ysize : currentStart + rowsPerThread;
        threads.emplace_back(initEdges, currentStart, currentEnd);
        currentStart = currentEnd;
    }

    for (auto &th : threads) {
        th.join();
    }

    // Initialize histogram and sum
    sum = 0;
    for (i = 0; i < 1000; i++) hist[i] = 0;
    fp2 = fimage2 + xsize + 1;
    for (j = 1; j < ysize - 1; j++) {
        for (i = 1; i < xsize - 1; i++) {
            if (*fp2 > *(fp2 - 1) && *fp2 > *(fp2 + 1) && *fp2 > *(fp2 - xsize) && *fp2 > *(fp2 + xsize)) {
                k = (int)(*fp2 * 1000.0f);
                if (k > 999) k = 999;
                if (k < 0) k = 0;
                hist[k]++;
                sum++;
            }
            fp2++;
        }
        fp2 += 2;
    }
    j = 0;
    for (i = 999; i >= 0; i--) {
        j += hist[i];
        if ((float)j / (float)(xsize * ysize) >= 0.02f) break;
    }
    k = i;
    ARLOGi("         ImageSize = %7d[pixel]\n", xsize * ysize);
    ARLOGi("Extracted features = %7d[pixel]\n", sum);
    ARLOGi(" Filtered features = %7d[pixel]\n", j);

    // Worker lambda to process a row chunk
    auto worker = [&](int startRow, int endRow) {
        for (int j = startRow; j < endRow; j++) {
            for (int i = 0; i < xsize; i++) {
                if (j == 0 || j == ysize - 1 || i == 0 || i == xsize - 1) {
                    fimage[j * xsize + i] = 1.0f;
                } else {
                    if (fimage2[j * xsize + i] <= fimage2[j * xsize + i - 1] || fimage2[j * xsize + i] <= fimage2[j * xsize + i + 1] ||
                        fimage2[j * xsize + i] <= fimage2[(j - 1) * xsize + i] || fimage2[j * xsize + i] <= fimage2[(j + 1) * xsize + i]) {
                        fimage[j * xsize + i] = 1.0f;
                    } else if ((int)(fimage2[j * xsize + i] * 1000) < k) {
                        fimage[j * xsize + i] = 1.0f;
                    } else {
                        if (make_template(image->imgBW, xsize, ysize, i, j, ts1, ts2, sd_thresh, tmpl, &vlen) < 0) {
                            fimage[j * xsize + i] = 1.0f;
                        } else {
                            max = -1.0f;
                            for (jj = -search_size1; jj <= search_size1; jj++) {
                                for (ii = -search_size1; ii <= search_size1; ii++) {
                                    if (ii * ii + jj * jj <= search_size2 * search_size2) continue;
                                    if (get_similarity(image->imgBW, xsize, ysize, tmpl, vlen, ts1, ts2, i + ii, j + jj, &sim) < 0) continue;
                                    if (sim > max) {
                                        max = sim;
                                        if (max > max_sim_thresh) break;
                                    }
                                }
                                if (max > max_sim_thresh) break;
                            }
                            fimage[j * xsize + i] = (float)max;
                        }
                    }
                }
            }
        }
    };

    // Create threads for feature map generation
    currentStart = 0;
    threads.clear();
    for (int t = 0; t < threadCount; t++) {
        int currentEnd = (t == threadCount - 1) ? ysize : currentStart + rowsPerThread;
        threads.emplace_back(worker, currentStart, currentEnd);
        currentStart = currentEnd;
    }

    for (auto &th : threads) {
        th.join();
    }

    // Populate and return the AR2FeatureMapT structure with fimage data
    arMalloc(featureMap, AR2FeatureMapT, 1);
    featureMap->map = fimage;
    featureMap->xsize = xsize;
    featureMap->ysize = ysize;

    free(fimage2);
    free(tmpl);

    return featureMap;
}
