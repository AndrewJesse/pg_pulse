ARG RUST_VERSION=1.99.0

FROM rust:${RUST_VERSION}-trixie AS builder
WORKDIR /src
COPY . .
RUN cargo build --release --manifest-path router/Cargo.toml

FROM gcr.io/distroless/cc-debian12:nonroot
WORKDIR /app
COPY --from=builder /src/target/release/router /app/router
USER 65532:65532
ENTRYPOINT ["/app/router"]
