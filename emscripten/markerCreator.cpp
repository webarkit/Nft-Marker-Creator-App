/*
 *  markerCreator.cpp
 *  Webarkit
 *
 *  Generates image sets and texture data.
 *
 *  Run with "--help" parameter to see usage.
 *
 *  This file is part of Webarkit.
 *
 *  Webarkit is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Webarkit is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Webarkit.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As a special exception, the copyright holders of this library give you
 *  permission to link this library with independent modules to produce an
 *  executable, regardless of the license terms of these independent modules, and to
 *  copy and distribute the resulting executable under terms of your choice,
 *  provided that you also meet, for each linked independent module, the terms and
 *  conditions of the license of that module. An independent module is a module
 *  which is neither derived from nor based on this library. If you modify this
 *  library, you may extend this exception to your version of the library, but you
 *  are not obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  Copyright 2015 Daqri, LLC.
 *  Copyright 2007-2015 ARToolworks, Inc.
 *  Copyright 2023 WebARKit org.
 *
 *  Author(s): Hirokazu Kato, Philip Lamb, Daniel Fernandez, Walter Perdan
 *
 *
 *
 */

#ifdef _WIN32
#include <windows.h>
#define truncf(x) floorf(x) // These are the same for positive numbers.
#endif
#include <emscripten/emscripten.h>
#include <stdio.h>
#include <string.h>
#include "AR/ar.h"
#include "AR2/config.h"
#include "AR2/imageFormat.h"
#include "AR2/imageSet.h"
#include "AR2/featureSet.h"
#include "AR2/util.h"
#include "KPM/kpm.h"
#ifdef _WIN32
#define MAXPATHLEN MAX_PATH
#else
#include <sys/param.h> // MAXPATHLEN
#endif
#if defined(__APPLE__) || defined(__linux__)
#define HAVE_DAEMON_FUNC 1
#include <unistd.h>
#endif
#include <time.h> // time(), localtime(), strftime()
#include <chrono>

#ifdef HAVE_THREADING
#include <thread>
#include <mutex>
#endif

#define KPM_SURF_FEATURE_DENSITY_L0 70
#define KPM_SURF_FEATURE_DENSITY_L1 100
#define KPM_SURF_FEATURE_DENSITY_L2 150
#define KPM_SURF_FEATURE_DENSITY_L3 200

#define TRACKING_EXTRACTION_LEVEL_DEFAULT 2
#define INITIALIZATION_EXTRACTION_LEVEL_DEFAULT 1
#define KPM_MINIMUM_IMAGE_SIZE 28 // Filter size for 1 octaves plus 1.
// #define KPM_MINIMUM_IMAGE_SIZE 196 // Filter size for 4 octaves plus 1.

#ifndef MIN
#define MIN(x, y) (x < y ? x : y)
#endif

enum
{
    E_NO_ERROR = 0,
    E_BAD_PARAMETER = 64,
    E_INPUT_DATA_ERROR = 65,
    E_USER_INPUT_CANCELLED = 66,
    E_BACKGROUND_OPERATION_UNSUPPORTED = 69,
    E_DATA_PROCESSING_ERROR = 70,
    E_UNABLE_TO_DETACH_FROM_CONTROLLING_TERMINAL = 71,
    E_GENERIC_ERROR = 255
};

static char pathToFiles[9] = "/marker/";

static int genfset = 1;
static int genfset3 = 1;

static char filename[MAXPATHLEN] = "";
static AR2JpegImageT *jpegImage;
// static ARUint8             *image;
static int xsize, ysize;
static int nc;
static float dpi = -1.0f;

static float dpiMin = -1.0f;
static float dpiMax = -1.0f;
static float *dpi_list;
static int dpi_num = 0;

static float sd_thresh = -1.0f;
static float min_thresh = -1.0f;
static float max_thresh = -1.0f;
static int featureDensity = -1;
static int occ_size = -1;
static int tracking_extraction_level = -1; // Allows specification from command-line.
static int initialization_extraction_level = -1;

static int background = 0;
static char logfile[MAXPATHLEN] = "";
static char exitcodefile[MAXPATHLEN] = "";
static char exitcode = -1;
#define EXIT(c)       \
    {                 \
        exitcode = c; \
        exit(c);      \
    }

#ifdef HAVE_THREADING
std::mutex m{};
#endif

static void usage(const char *com);
static int setDPI(void);

extern "C"
{

    int createNftDataSet(ARUint8 *imageIn, float dpiIn, int xsizeIn, int ysizeIn, int ncIn, char *cmdStr)
    {
        AR2JpegImageT *jpegImage = NULL;
        ARUint8 *image = NULL;
        AR2ImageSetT *imageSet = NULL;
        AR2FeatureMapT *featureMap = NULL;
        AR2FeatureSetT *featureSet = NULL;
        KpmRefDataSet *refDataSet = NULL;
        float scale1, scale2;
        int procMode;
        char buf[1024];
        int num;
        int i, j;
        char *sep = NULL;
        time_t clock;
        int maxFeatureNum;
        int err;

        dpi = dpiIn;
        xsize = xsizeIn;
        ysize = ysizeIn;
        nc = ncIn;
        image = imageIn;

        int cmdLen = strlen(cmdStr);
        ARLOGi("\nCommands: %s\n", cmdStr);

        if (strstr(cmdStr, "-dpi=") != NULL)
        {
            char *result = strstr(cmdStr, "-dpi=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 5], "%f", &dpi);
        }
        if (strstr(cmdStr, "-sd_thresh=") != NULL)
        {
            char *result = strstr(cmdStr, "-sd_thresh=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 11], "%f", &sd_thresh);
        }
        if (strstr(cmdStr, "-max_thresh=") != NULL)
        {
            char *result = strstr(cmdStr, "-max_thresh=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 15], "%f", &max_thresh);
        }
        if (strstr(cmdStr, "-min_thresh=") != NULL)
        {
            char *result = strstr(cmdStr, "-min_thresh=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 12], "%f", &min_thresh);
        }
        if (strstr(cmdStr, "-feature_density=") != NULL)
        {
            char *result = strstr(cmdStr, "-feature_density=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 13], "%d", &featureDensity);
        }
        if (strstr(cmdStr, "-level=") != NULL)
        {
            char *result = strstr(cmdStr, "-level=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 7], "%d", &tracking_extraction_level);
        }
        if (strstr(cmdStr, "-leveli=") != NULL)
        {
            char *result = strstr(cmdStr, "-leveli=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 8], "%d", &initialization_extraction_level);
        }
        if (strstr(cmdStr, "-max_dpi=") != NULL)
        {
            char *result = strstr(cmdStr, "-max_dpi=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 9], "%f", &dpiMax);
        }
        if (strstr(cmdStr, "-min_dpi=") != NULL)
        {
            char *result = strstr(cmdStr, "-min_dpi=");
            int pos = (result - cmdStr);
            sscanf(&cmdStr[pos + 9], "%f", &dpiMin);
        }
        if (strstr(cmdStr, "-background") != NULL)
        {
            background = 1;
        }
        if (strstr(cmdStr, "-nofset") != NULL)
        {
            genfset = 0;
        }
        if (strstr(cmdStr, "-fset") != NULL)
        {
            genfset = 1;
        }
        if (strstr(cmdStr, "-nofset3") != NULL)
        {
            genfset3 = 0;
        }
        if (strstr(cmdStr, "-fset3") != NULL)
        {
            genfset3 = 1;
        }
        if (strstr(cmdStr, "--help") != NULL || strstr(cmdStr, "-h") != NULL || strstr(cmdStr, "-?") != NULL)
        {
            usage("Marker Generator");
        }

        char *filename = strdup("tempFilename");

        // Print the start date and time.
        clock = time(NULL);
        if (clock != (time_t)-1)
        {
            struct tm *timeptr = localtime(&clock);
            if (timeptr)
            {
                char stime[26 + 8] = "";
                if (strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S %z", timeptr)) // e.g. "1999-12-31 23:59:59 NZDT".
                    ARLOGi("--\nGenerator started at %s\n", stime);
            }
        }

        std::chrono::high_resolution_clock::time_point duration_start = std::chrono::high_resolution_clock::now();

        if (genfset)
        {
            if (tracking_extraction_level == -1)
            {
                tracking_extraction_level = TRACKING_EXTRACTION_LEVEL_DEFAULT;
            }
            switch (tracking_extraction_level)
            {
            case 0:
                if (sd_thresh == -1.0f)
                    sd_thresh = AR2_DEFAULT_SD_THRESH_L0;
                if (min_thresh == -1.0f)
                    min_thresh = AR2_DEFAULT_MIN_SIM_THRESH_L0;
                if (max_thresh == -1.0f)
                    max_thresh = AR2_DEFAULT_MAX_SIM_THRESH_L0;
                if (occ_size == -1)
                    occ_size = AR2_DEFAULT_OCCUPANCY_SIZE;
                break;
            case 1:
                if (sd_thresh == -1.0f)
                    sd_thresh = AR2_DEFAULT_SD_THRESH_L1;
                if (min_thresh == -1.0f)
                    min_thresh = AR2_DEFAULT_MIN_SIM_THRESH_L1;
                if (max_thresh == -1.0f)
                    max_thresh = AR2_DEFAULT_MAX_SIM_THRESH_L1;
                if (occ_size == -1)
                    occ_size = AR2_DEFAULT_OCCUPANCY_SIZE;
                break;
            case 2:
                if (sd_thresh == -1.0f)
                    sd_thresh = AR2_DEFAULT_SD_THRESH_L2;
                if (min_thresh == -1.0f)
                    min_thresh = AR2_DEFAULT_MIN_SIM_THRESH_L2;
                if (max_thresh == -1.0f)
                    max_thresh = AR2_DEFAULT_MAX_SIM_THRESH_L2;
                if (occ_size == -1)
                    occ_size = AR2_DEFAULT_OCCUPANCY_SIZE * 2 / 3;
                break;
            case 3:
                if (sd_thresh == -1.0f)
                    sd_thresh = AR2_DEFAULT_SD_THRESH_L3;
                if (min_thresh == -1.0f)
                    min_thresh = AR2_DEFAULT_MIN_SIM_THRESH_L3;
                if (max_thresh == -1.0f)
                    max_thresh = AR2_DEFAULT_MAX_SIM_THRESH_L3;
                if (occ_size == -1)
                    occ_size = AR2_DEFAULT_OCCUPANCY_SIZE * 2 / 3;
                break;
            case 4: // Same as 3, but with smaller AR2_DEFAULT_OCCUPANCY_SIZE.
                if (sd_thresh == -1.0f)
                    sd_thresh = AR2_DEFAULT_SD_THRESH_L3;
                if (min_thresh == -1.0f)
                    min_thresh = AR2_DEFAULT_MIN_SIM_THRESH_L3;
                if (max_thresh == -1.0f)
                    max_thresh = AR2_DEFAULT_MAX_SIM_THRESH_L3;
                if (occ_size == -1)
                    occ_size = AR2_DEFAULT_OCCUPANCY_SIZE * 1 / 2;
                break;
            default: // We only get to here if the parameters are already set.
                break;
            }
            ARLOGi("Tracking Extraction Level = %d\n", tracking_extraction_level);
            ARLOGi("MAX_THRESH  = %f\n", max_thresh);
            ARLOGi("MIN_THRESH  = %f\n", min_thresh);
            ARLOGi("SD_THRESH   = %f\n", sd_thresh);
        }
        if (genfset3)
        {
            if (initialization_extraction_level == -1 && featureDensity == -1)
            {
                initialization_extraction_level = INITIALIZATION_EXTRACTION_LEVEL_DEFAULT;
            }
            switch (initialization_extraction_level)
            {
            case 0:
                if (featureDensity == -1)
                    featureDensity = KPM_SURF_FEATURE_DENSITY_L0;
                break;
            default:
            case 1:
                if (featureDensity == -1)
                    featureDensity = KPM_SURF_FEATURE_DENSITY_L1;
                break;
            case 2:
                if (featureDensity == -1)
                    featureDensity = KPM_SURF_FEATURE_DENSITY_L2;
                break;
            case 3:
                if (featureDensity == -1)
                    featureDensity = KPM_SURF_FEATURE_DENSITY_L3;
                break;
            }
            ARLOGi("Initialization Extraction Level = %d\n", initialization_extraction_level);
            ARLOGi("SURF_FEATURE = %d\n", featureDensity);
        }

        setDPI();

        ARLOGi("Generating ImageSet...\n");
        ARLOGi("   (Source image xsize=%d, ysize=%d, channels=%d, dpi=%.1f).\n", xsize, ysize, nc, dpi);
#ifdef HAVE_THREADING
        ARLOGi("   (Threading enabled).\n");
        m.lock();
#endif
        imageSet = ar2GenImageSet(image, xsize, ysize, nc, dpi, dpi_list, dpi_num);
#ifdef HAVE_THREADING
        m.unlock();
#endif
        ar2FreeJpegImage(&jpegImage);
        if (imageSet == NULL)
        {
            ARLOGe("ImageSet generation error!!\n");
            EXIT(E_DATA_PROCESSING_ERROR);
        }
        ARLOGi("  Done.\n");

        // ar2UtilRemoveExt( filename );
        ARLOGi("Saving to %s.iset...\n", filename);
        if (ar2WriteImageSet(filename, imageSet) < 0)
        {
            ARLOGe("Save error: %s.iset\n", filename);
            EXIT(E_DATA_PROCESSING_ERROR);
        }
        ARLOGi("  Done.\n");

        if (genfset)
        {
            arMalloc(featureSet, AR2FeatureSetT, 1);                      // A featureSet with a single image,
            arMalloc(featureSet->list, AR2FeaturePointsT, imageSet->num); // and with 'num' scale levels of this image.
            featureSet->num = imageSet->num;

            ARLOGi("Generating FeatureList...\n");

            for (i = 0; i < imageSet->num; i++)
            {
                ARLOGi("Start for %f dpi image.\n", imageSet->scale[i]->dpi);

                #ifdef HAVE_THREADING
                    m.lock();
                #endif

                featureMap = ar2GenFeatureMap(imageSet->scale[i],
                                              AR2_DEFAULT_TS1 * AR2_TEMP_SCALE, AR2_DEFAULT_TS2 * AR2_TEMP_SCALE,
                                              AR2_DEFAULT_GEN_FEATURE_MAP_SEARCH_SIZE1, AR2_DEFAULT_GEN_FEATURE_MAP_SEARCH_SIZE2,
                                              AR2_DEFAULT_MAX_SIM_THRESH2, AR2_DEFAULT_SD_THRESH2);

                #ifdef HAVE_THREADING
                    m.unlock();
                #endif

                if (featureMap == NULL)
                {
                    ARLOGe("Error!!\n");
                    EXIT(E_DATA_PROCESSING_ERROR);
                }
                ARLOGi("  Done.\n");

                featureSet->list[i].coord = ar2SelectFeature2(imageSet->scale[i], featureMap,
                                                              AR2_DEFAULT_TS1 * AR2_TEMP_SCALE, AR2_DEFAULT_TS2 * AR2_TEMP_SCALE, AR2_DEFAULT_GEN_FEATURE_MAP_SEARCH_SIZE2,
                                                              occ_size,
                                                              max_thresh, min_thresh, sd_thresh, &num);
                if (featureSet->list[i].coord == NULL)
                    num = 0;
                featureSet->list[i].num = num;
                featureSet->list[i].scale = i;

                scale1 = 0.0f;
                for (j = 0; j < imageSet->num; j++)
                {
                    if (imageSet->scale[j]->dpi < imageSet->scale[i]->dpi)
                    {
                        if (imageSet->scale[j]->dpi > scale1)
                            scale1 = imageSet->scale[j]->dpi;
                    }
                }
                if (scale1 == 0.0f)
                {
                    featureSet->list[i].mindpi = imageSet->scale[i]->dpi * 0.5f;
                }
                else
                {
                    /*
                     scale2 = imageSet->scale[i]->dpi;
                     scale = sqrtf( scale1 * scale2 );
                     featureSet->list[i].mindpi = scale2 / ((scale2/scale - 1.0f)*1.1f + 1.0f);
                     */
                    featureSet->list[i].mindpi = scale1;
                }

                scale1 = 0.0f;
                for (j = 0; j < imageSet->num; j++)
                {
                    if (imageSet->scale[j]->dpi > imageSet->scale[i]->dpi)
                    {
                        if (scale1 == 0.0f || imageSet->scale[j]->dpi < scale1)
                            scale1 = imageSet->scale[j]->dpi;
                    }
                }
                if (scale1 == 0.0f)
                {
                    featureSet->list[i].maxdpi = imageSet->scale[i]->dpi * 2.0f;
                }
                else
                {
                    // scale2 = imageSet->scale[i]->dpi * 1.2f;
                    scale2 = imageSet->scale[i]->dpi;
                    /*
                     scale = sqrtf( scale1 * scale2 );
                     featureSet->list[i].maxdpi = scale2 * ((scale/scale2 - 1.0f)*1.1f + 1.0f);
                     */
                    featureSet->list[i].maxdpi = scale2 * 0.8f + scale1 * 0.2f;
                }

                ar2FreeFeatureMap(featureMap);
            }
            ARLOGi("  Done.\n");

            ARLOGi("Saving FeatureSet...\n");
            if (ar2SaveFeatureSet(filename, strdup("fset"), featureSet) < 0)
            {
                ARLOGe("Save error: %s.fset\n", filename);
                EXIT(E_DATA_PROCESSING_ERROR);
            }
            ARLOGi("  Done.\n");
            ar2FreeFeatureSet(&featureSet);
        }

        if (genfset3)
        {
            ARLOGi("Generating FeatureSet3...\n");
            refDataSet = NULL;
            procMode = KpmProcFullSize;
            for (i = 0; i < imageSet->num; i++)
            {
                // if( imageSet->scale[i]->dpi > 100.0f ) continue;

                maxFeatureNum = featureDensity * imageSet->scale[i]->xsize * imageSet->scale[i]->ysize / (480 * 360);
                ARLOGi("(%d, %d) %f[dpi]\n", imageSet->scale[i]->xsize, imageSet->scale[i]->ysize, imageSet->scale[i]->dpi);
                if (kpmAddRefDataSet(
#if AR2_CAPABLE_ADAPTIVE_TEMPLATE
                        imageSet->scale[i]->imgBWBlur[1],
#else
                        imageSet->scale[i]->imgBW,
#endif
                        imageSet->scale[i]->xsize,
                        imageSet->scale[i]->ysize,
                        imageSet->scale[i]->dpi,
                        procMode, KpmCompNull, maxFeatureNum, 1, i, &refDataSet) < 0)
                { // Page number set to 1 by default.
                    ARLOGe("Error at kpmAddRefDataSet.\n");
                    EXIT(E_DATA_PROCESSING_ERROR);
                }
            }
            ARLOGi("  Done.\n");
            ARLOGi("Saving FeatureSet3...\n");
            if (kpmSaveRefDataSet(filename, "fset3", refDataSet) != 0)
            {
                ARLOGe("Save error: %s.fset2\n", filename);
                EXIT(E_DATA_PROCESSING_ERROR);
            }

            ARLOGi("  Done.\n");
            kpmDeleteRefDataSet(&refDataSet);
        }

        ar2FreeImageSet(&imageSet);

        // Print the start date and time.
        clock = time(NULL);
        if (clock != (time_t)-1)
        {
            struct tm *timeptr = localtime(&clock);
            if (timeptr)
            {
                char stime[26 + 8] = "";
                if (strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S %z", timeptr)) // e.g. "1999-12-31 23:59:59 NZDT".
                    ARLOGi("Generator finished at %s\n--\n", stime);
            }
        }

        std::chrono::high_resolution_clock::time_point duration_end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> duration = (duration_end - duration_start);

        ARLOGi("NFTMarkerGenerator took %f seconds to execute\n", duration.count());

        exitcode = E_NO_ERROR;
        return (exitcode);
    }

    // Reads dpiMinAllowable, xsize, ysize, dpi, background, dpiMin, dpiMax.
    // Sets dpiMin, dpiMax, dpi_num, dpi_list.
    static int setDPI(void)
    {
        float dpiWork, dpiMinAllowable;
        char buf1[256];
        int i;

        // Determine minimum allowable DPI, truncated to 3 decimal places.
        dpiMinAllowable = truncf(((float)KPM_MINIMUM_IMAGE_SIZE / (float)(MIN(xsize, ysize))) * dpi * 1000.0) / 1000.0f;
        ARLOGi(" min allow %f.\n", dpiMinAllowable);
        if (background)
        {
            if (dpiMin == -1.0f)
                dpiMin = dpiMinAllowable;
            if (dpiMax == -1.0f)
                dpiMax = dpi;
        }

        if (dpiMin == -1.0f)
        {
            dpiMin = dpiMinAllowable;
        }
        else if (dpiMin < dpiMinAllowable)
        {
            ARLOGe("Warning: -min_dpi=%.3f smaller than minimum allowable. Value will be adjusted to %.3f.\n", dpiMin, dpiMinAllowable);
            dpiMin = dpiMinAllowable;
        }
        if (dpiMax == -1.0f)
        {
            dpiMax = dpi;
        }
        else if (dpiMax > dpi)
        {
            ARLOGe("Warning: -max_dpi=%.3f larger than maximum allowable. Value will be adjusted to %.3f.\n", dpiMax, dpi);
            dpiMax = dpi;
        }
        // Decide how many levels we need.
        if (dpiMin == dpiMax)
        {
            dpi_num = 1;
        }
        else
        {
            dpiWork = dpiMin;
            for (i = 1;; i++)
            {
                dpiWork *= powf(2.0f, 1.0f / 3.0f); // *= 1.25992104989487
                if (dpiWork >= dpiMax * 0.95f)
                {
                    break;
                }
                dpi_num++;
            }
            dpi_num = i + 1;
        }

        arMalloc(dpi_list, float, dpi_num);

        // Determine the DPI values of each level.
        dpiWork = dpiMin;
        for (int i = 0; i < dpi_num; i++)
        {
            ARLOGi("Image DPI (%d): %f\n", i + 1, dpiWork);
            dpi_list[dpi_num - i - 1] = dpiWork; // Lowest value goes at tail of array, highest at head.
            dpiWork *= powf(2.0f, 1.0f / 3.0f);
            if (dpiWork >= dpiMax * 0.95f)
                dpiWork = dpiMax;
        }

        return 0;
    }

    static void usage(const char *com)
    {
        if (!background)
        {
            ARLOG("%s <filename>\n", com);
            ARLOG("    -level=n\n"
                  "         (n is an integer in range 0 (few) to 4 (many). Default %d.'\n",
                  TRACKING_EXTRACTION_LEVEL_DEFAULT);
            ARLOG("    -sd_thresh=<sd_thresh>\n");
            ARLOG("    -max_thresh=<max_thresh>\n");
            ARLOG("    -min_thresh=<min_thresh>\n");
            ARLOG("    -leveli=n\n"
                  "         (n is an integer in range 0 (few) to 3 (many). Default %d.'\n",
                  INITIALIZATION_EXTRACTION_LEVEL_DEFAULT);
            ARLOG("    -feature_density=<feature_density>\n");
            ARLOG("    -dpi=f: Override embedded JPEG DPI value.\n");
            ARLOG("    -max_dpi=<max_dpi>\n");
            ARLOG("    -min_dpi=<min_dpi>\n");
            ARLOG("    -background\n");
            ARLOG("         Run in background, i.e. as daemon detached from controlling terminal. (macOS and Linux only.)\n");
            ARLOG("    -log=<path>\n");
            ARLOG("    -loglevel=x\n");
            ARLOG("         x is one of: DEBUG, INFO, WARN, ERROR. Default is %s.\n", (AR_LOG_LEVEL_DEFAULT == AR_LOG_LEVEL_DEBUG ? "DEBUG" : (AR_LOG_LEVEL_DEFAULT == AR_LOG_LEVEL_INFO ? "INFO" : (AR_LOG_LEVEL_DEFAULT == AR_LOG_LEVEL_WARN ? "WARN" : (AR_LOG_LEVEL_DEFAULT == AR_LOG_LEVEL_ERROR ? "ERROR" : "UNKNOWN")))));
            ARLOG("    -exitcode=<path>\n");
            ARLOG("    --help -h -?  Display this help\n");
        }

        EXIT(E_BAD_PARAMETER);
    }

} // extern "C"