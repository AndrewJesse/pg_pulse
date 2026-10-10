resource "aws_scheduler_schedule" "stop" {
  name = "pg-pulse-stop"
  schedule_expression = var.auto_stop_cron
  flexible_time_window { mode = "OFF" }
  target {
    arn      = "arn:aws:scheduler:::aws-sdk:ec2:stopInstances"
    role_arn = aws_iam_role.scheduler.arn
    input = jsonencode({
      InstanceIds = [aws_instance.main.id]
    })
  }
}

resource "aws_iam_role" "scheduler" {
  name = "pg-pulse-scheduler-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect = "Allow"
      Principal = { Service = "scheduler.amazonaws.com" }
      Action = "sts:AssumeRole"
    }]
  })
}
