PG_MAJOR ?= 18
TAG ?= dev
PG_CONFIG ?= pg_config
CARGO ?= cargo
PGRX_RUSTFLAGS ?=

ifeq ($(shell uname -s),Darwin)
PGRX_RUSTFLAGS = -C link-arg=-undefined -C link-arg=dynamic_lookup
endif

.PHONY: build build-c build-rust build-health test test-c test-rust test-health lint image image-chaos up down kind-up argocd-up dev-deploy e2e chaos aws-bootstrap aws-up aws-tunnel aws-pause aws-resume aws-status aws-recreate aws-destroy clean

build: build-c build-rust build-health

build-c:
	$(MAKE) -C extensions/pulse_core PG_CONFIG=$(PG_CONFIG)
	$(MAKE) -C extensions/pulse_decoder PG_CONFIG=$(PG_CONFIG)

build-rust:
	$(CARGO) build --workspace

build-health:
	RUSTFLAGS="$(PGRX_RUSTFLAGS)" $(CARGO) rustc --manifest-path extensions/pulse_health/Cargo.toml --lib --release --features pg$(PG_MAJOR)

test: test-c test-rust test-health

test-c:
	$(MAKE) -C extensions/pulse_core install PG_CONFIG=$(PG_CONFIG)
	$(MAKE) -C extensions/pulse_core installcheck PG_CONFIG=$(PG_CONFIG)

test-rust:
	$(CARGO) test --workspace

test-health:
	$(PGRX) test pg$(PG_MAJOR) --manifest-path extensions/pulse_health/Cargo.toml

lint:
	$(CARGO) fmt --all -- --check
	$(CARGO) clippy --workspace --all-targets -- -D warnings
	clang-format --dry-run --Werror $$(find extensions -path '*/target' -prune -o \( -name '*.c' -o -name '*.h' \) -print)
	cppcheck --error-exitcode=1 --enable=warning,style,performance,portability extensions/pulse_core extensions/pulse_decoder
	hadolint docker/postgres/Dockerfile docker/postgres/Dockerfile.chaos docker/router/Dockerfile docker/rules-engine/Dockerfile
	shellcheck scripts/*.sh chaos/*.sh
	yamllint .github deploy scripts infra

image:
	@echo "TODO: build deployable images for $(PG_MAJOR)"

image-chaos:
	@echo "TODO: build chaos image"

up:
	docker compose up -d

down:
	docker compose down -v

kind-up:
	@echo "TODO: create kind cluster"

argocd-up:
	@echo "TODO: install Argo CD and root app"

dev-deploy:
	@echo "TODO: apply local dev overlay"

e2e:
	@echo "TODO: run e2e script"

chaos:
	@echo "TODO: run chaos suite"

aws-bootstrap:
	@echo "TODO: terraform apply infra/bootstrap"

aws-up:
	@echo "TODO: terraform apply infra/aws"

aws-tunnel:
	@echo "TODO: fetch kubeconfig from SSM and port-forward 6443"

aws-pause:
	@echo "TODO: stop the EC2 instance"

aws-resume:
	@echo "TODO: start the EC2 instance"

aws-status:
	@echo "TODO: print instance status and uptime"

aws-recreate:
	@echo "TODO: destroy and recreate the AWS environment"

aws-destroy:
	@echo "TODO: terraform destroy infra/aws"

clean:
	$(MAKE) -C extensions/pulse_core clean PG_CONFIG=$(PG_CONFIG)
	$(MAKE) -C extensions/pulse_decoder clean PG_CONFIG=$(PG_CONFIG)
	$(CARGO) clean --workspace
	$(CARGO) clean --manifest-path extensions/pulse_health/Cargo.toml
