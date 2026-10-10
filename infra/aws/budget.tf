resource "aws_budgets_budget" "monthly" {
  name = "pg-pulse-budget"
  budget_type = "COST"
  limit_amount = tostring(var.budget_usd)
  limit_unit = "USD"
  time_unit = "MONTHLY"

  notification {
    comparison_operator        = "GREATER_THAN"
    threshold                  = 50
    threshold_type             = "PERCENTAGE"
    notification_type          = "FORECASTED"
    subscriber_email_addresses = [var.budget_email]
  }
}
