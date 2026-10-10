# 0001: Record architecture decisions

This repository records architecture decisions in ADR format.

## Status

Accepted.

## Context

The system is a small, pausable Postgres health and routing environment built to run on a single k3s node.

## Decision

Use GitOps with Argo CD, CloudNativePG, and GHCR, with a single AWS EC2 instance as the cost-controlled host.
