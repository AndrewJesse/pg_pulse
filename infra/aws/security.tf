resource "aws_security_group" "main" {
  name        = "pg-pulse"
  description = "No inbound access; egress only"
  vpc_id      = aws_vpc.main.id

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}
