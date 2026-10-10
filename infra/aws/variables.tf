variable "region" {
  type    = string
  default = "us-east-1"
}

variable "instance_type" {
  type    = string
  default = "t4g.large"
}

variable "use_spot" {
  type    = bool
  default = false
}

variable "root_volume_gb" {
  type    = number
  default = 30
}

variable "k3s_version" {
  type = string
}

variable "argocd_version" {
  type = string
}

variable "repo_url" {
  type = string
}

variable "git_ref" {
  type    = string
  default = "main"
}

variable "auto_stop_cron" {
  type    = string
  default = "cron(0 2 * * ? *)"
}

variable "auto_stop_timezone" {
  type    = string
  default = "UTC"
}

variable "budget_usd" {
  type    = number
  default = 15
}

variable "budget_email" {
  type = string
}

variable "enable_idle_stop_alarm" {
  type    = bool
  default = false
}
