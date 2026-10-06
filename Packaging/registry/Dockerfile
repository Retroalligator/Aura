# The image is an archive of native installers and corresponding source.
# It deliberately contains no Linux application or executable entrypoint.
FROM scratch
ARG AURA_VERSION
ARG AURA_REVISION
LABEL org.opencontainers.image.title="Aura release bundle" \
      org.opencontainers.image.description="Native macOS/Windows audio plugin installers, complete source, validation evidence and checksums" \
      org.opencontainers.image.source="https://github.com/Retroalligator/Aura" \
      org.opencontainers.image.url="https://github.com/Retroalligator/Aura/releases" \
      org.opencontainers.image.licenses="AGPL-3.0-only" \
      org.opencontainers.image.version="${AURA_VERSION}" \
      org.opencontainers.image.revision="${AURA_REVISION}"
COPY payloads/ /aura/
