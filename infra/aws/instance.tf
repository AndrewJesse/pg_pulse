resource "aws_instance" "main" {
  ami           = data.aws_ssm_parameter.ubuntu_24_arm64.value
  instance_type = var.instance_type
  subnet_id     = aws_subnet.public.id
  vpc_security_group_ids = [aws_security_group.main.id]
  iam_instance_profile = aws_iam_instance_profile.main.name

  metadata_options {
    http_tokens = "required"
  }

  root_block_device {
    volume_type = "gp3"
    volume_size = var.root_volume_gb
    encrypted   = true
  }

  credit_specification {
    cpu_credits = "standard"
  }

  user_data = file("${path.module}/templates/user-data.sh.tftpl")

  tags = {
    Name    = "pg-pulse"
    Project = "pg-pulse"
  }
}

data "aws_ssm_parameter" "ubuntu_24_arm64" {
  name = "/aws/service/canonical/ubuntu/server/24.04/stable/current/arm64/hvm/ebs-gp3/ami-id"
}
