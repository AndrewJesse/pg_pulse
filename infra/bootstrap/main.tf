terraform {
  required_version = ">= 1.10.0"
}

provider "aws" {
  region = "us-east-1"
}

resource "aws_s3_bucket" "state" {
  bucket = "pg-pulse-terraform-state"
}
