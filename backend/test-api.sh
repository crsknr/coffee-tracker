#!/bin/bash

# =============================================================================
# Coffee Tracker API — Test Script
# Version: 1.3
#
# Prerequisites:
#   - jq installed (brew install jq / apt install jq)
#   - Run initializeSheets() and addSampleData() in Apps Script first
#   - Deploy as Web App and paste URL below
#
# Note: Google Apps Script redirects POST requests (302) to a
# googleusercontent.com URL. The POST body is consumed by the script
# but the response is only available by GETting the redirect URL.
# The post() helper handles this two-step process.
# =============================================================================

API_URL="https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec"

# Unique prefix for this test run
RUN_ID="test-$(date +%s)"

# Colors for output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

# Helper for POST requests
# Step 1: POST the data (script executes, response is encoded in redirect URL)
# Step 2: GET the redirect URL to retrieve the JSON response
post() {
  local REDIRECT
  REDIRECT=$(curl -s -o /dev/null -w '%{redirect_url}' \
    -X POST \
    -H "Content-Type: application/json" \
    -d "$1" \
    "$API_URL")
  
  if [ -z "$REDIRECT" ]; then
    echo '{"error": "No redirect URL received"}'
    return 1
  fi
  
  curl -sL "$REDIRECT"
}

echo ""
echo "=========================================="
echo "  ☕ Coffee Tracker API Test Suite"
echo "  Run ID: $RUN_ID"
echo "=========================================="
echo ""

# --- GET Tests ---

echo -e "${CYAN}--- GET Endpoints ---${NC}"
echo ""

echo -e "${GREEN}1. getConfig (boot/sync data):${NC}"
curl -sL "$API_URL?action=getConfig" | jq .
echo ""

echo -e "${GREEN}2. getUsers:${NC}"
curl -sL "$API_URL?action=getUsers" | jq .
echo ""

echo -e "${GREEN}3. getInventory:${NC}"
curl -sL "$API_URL?action=getInventory" | jq .
echo ""

# --- POST: Consume ---

echo -e "${CYAN}--- Consume ---${NC}"
echo ""

echo -e "${GREEN}4. Christoph orders 1 Double + 2 Oat Milk Portions:${NC}"
post '{
  "user": "Christoph",
  "type": "consume",
  "items": ["Double", "Oat Milk Portion"],
  "quantities": [1, 2],
  "transactionId": "'$RUN_ID'-consume-1"
}' | jq .
echo ""

echo -e "${GREEN}5. Laura orders 1 Single + 1 Cow Milk Portion:${NC}"
post '{
  "user": "Laura",
  "type": "consume",
  "items": ["Single", "Cow Milk Portion"],
  "quantities": [1, 1],
  "transactionId": "'$RUN_ID'-consume-2"
}' | jq .
echo ""

# --- POST: Purchase ---

echo -e "${CYAN}--- Purchase ---${NC}"
echo ""

echo -e "${GREEN}6. Christoph buys 2 Beans + 6 Oat Milk Cartons:${NC}"
post '{
  "user": "Christoph",
  "type": "purchase",
  "items": ["Beans", "Oat Milk Carton"],
  "quantities": [2, 6],
  "transactionId": "'$RUN_ID'-purchase-1"
}' | jq .
echo ""

# --- POST: Settle ---

echo -e "${CYAN}--- Settle ---${NC}"
echo ""

echo -e "${GREEN}7. Laura settles €0.50 via PayPal:${NC}"
post '{
  "user": "Laura",
  "type": "settle",
  "amount": 0.50,
  "transactionId": "'$RUN_ID'-settle-1"
}' | jq .
echo ""

# --- POST: Undo ---

echo -e "${CYAN}--- Undo ---${NC}"
echo ""

echo -e "${GREEN}8. Laura undoes her last transaction (the settle):${NC}"
post '{
  "user": "Laura",
  "type": "undo",
  "transactionId": "'$RUN_ID'-undo-1"
}' | jq .
echo ""

echo -e "${GREEN}9. Undo again (should fail — last is already an undo):${NC}"
post '{
  "user": "Laura",
  "type": "undo",
  "transactionId": "'$RUN_ID'-undo-2"
}' | jq .
echo ""

# --- Duplicate Handling ---

echo -e "${CYAN}--- Duplicate Handling ---${NC}"
echo ""

echo -e "${GREEN}10. Replay Christoph consume (same transactionId — should return duplicate):${NC}"
post '{
  "user": "Christoph",
  "type": "consume",
  "items": ["Double", "Oat Milk Portion"],
  "quantities": [1, 2],
  "transactionId": "'$RUN_ID'-consume-1"
}' | jq .
echo ""

# --- Error Handling ---

echo -e "${CYAN}--- Error Handling ---${NC}"
echo ""

echo -e "${GREEN}11. Unknown item (should fail):${NC}"
post '{
  "user": "Christoph",
  "type": "consume",
  "items": ["Triple Espresso"],
  "quantities": [1],
  "transactionId": "'$RUN_ID'-error-1"
}' | jq .
echo ""

echo -e "${GREEN}12. Unknown user (should fail):${NC}"
post '{
  "user": "Ghost",
  "type": "consume",
  "items": ["Single"],
  "quantities": [1],
  "transactionId": "'$RUN_ID'-error-2"
}' | jq .
echo ""

echo -e "${GREEN}13. Missing transactionId (should fail):${NC}"
post '{
  "user": "Christoph",
  "type": "consume",
  "items": ["Single"],
  "quantities": [1]
}' | jq .
echo ""

echo -e "${GREEN}14. Items/quantities mismatch (should fail):${NC}"
post '{
  "user": "Christoph",
  "type": "consume",
  "items": ["Single", "Double"],
  "quantities": [1],
  "transactionId": "'$RUN_ID'-error-3"
}' | jq .
echo ""

# --- Final State ---

echo -e "${CYAN}--- Final State ---${NC}"
echo ""

echo -e "${GREEN}15. Users (final balances):${NC}"
curl -sL "$API_URL?action=getUsers" | jq .
echo ""

echo -e "${GREEN}16. Inventory (final stock):${NC}"
curl -sL "$API_URL?action=getInventory" | jq .
echo ""

echo -e "${GREEN}17. Christoph's last 10 transactions:${NC}"
curl -sL "$API_URL?action=getLastTransactions&user=Christoph&limit=10" | jq .
echo ""

echo -e "${GREEN}18. Laura's last 10 transactions:${NC}"
curl -sL "$API_URL?action=getLastTransactions&user=Laura&limit=10" | jq .
echo ""

echo "=========================================="
echo "  ☕ Test Complete"
echo "=========================================="