FROM ubuntu:22.04
LABEL authors="kalwalt"

#ENV HOME="/root"
ARG NVM_VERSION=v0.39.7
ENV NVM_DIR=/usr/local/nvm
ENV NODE_VERSION 20.17.0

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential\
        libssl-dev \
        git \
        curl \
        ca-certificates \
    && mkdir /app/ \
    && mkdir /config/ \
    && mkdir /scripts/ \
    && mkdir -p ${NVM_DIR} \
    && curl -o- "https://raw.githubusercontent.com/nvm-sh/nvm/${NVM_VERSION}/install.sh" | bash \
    && echo ". ${NVM_DIR}/nvm.sh --no-use && nvm \"\$@\"" > /usr/local/bin/nvm \
    && chmod +x /usr/local/bin/nvm

# Update the $PATH to make your installed `node` and `npm` available!
ENV PATH      $NVM_DIR/versions/node/v$NODE_VERSION/bin:$PATH

WORKDIR /src/

COPY . /Nft-Marker-Creator-App
RUN npm -v
# Install node dependency
RUN cd /Nft-Marker-Creator-App && ls

RUN npm install --prefix /Nft-Marker-Creator-App

ENV PATH  /Nft-Marker-Creator-App/src:$PATH

COPY docker/scripts /scripts/

#RUN chmod +x /scripts/docker/entrypoint.sh
ENTRYPOINT ["/scripts/docker/entrypoint.sh"]