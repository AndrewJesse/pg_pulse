terraform {
  backend "s3" {
    bucket       = "pg-pulse-terraform-state"
    key          = "pg-pulse/dev.tfstate"
    region       = "us-east-1"
    use_lockfile = true
  }
}
