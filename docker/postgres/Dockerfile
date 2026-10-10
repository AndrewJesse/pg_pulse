ARG PG_MAJOR=18
ARG PG_VERSION=18.6

FROM postgres:${PG_MAJOR}-trixie AS builder
ARG DEBIAN_FRONTEND=noninteractive
# hadolint ignore=DL3008
RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		build-essential \
		clang \
		libclang-dev \
		"postgresql-server-dev-${PG_MAJOR}" \
		curl \
		ca-certificates \
		git \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src
RUN echo "TODO: build pulse_core and pulse_decoder with PGXS" &&     echo "TODO: build pulse_health with cargo pgrx package --pg-config $(which pg_config)"

FROM ghcr.io/cloudnative-pg/postgresql:${PG_VERSION}-standard-trixie AS runtime
COPY --from=builder /src /tmp/src
RUN echo "TODO: install .so and extension SQL into the runtime image"
