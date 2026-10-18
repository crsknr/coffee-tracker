// =============================================================================
// Coffee Tracker — Google Apps Script Backend
// Version: 1.3
// =============================================================================

// --- Sheet Names ---
const SHEET_TRANSACTIONS = 'Transactions';
const SHEET_USERS = 'Users';
const SHEET_INVENTORY = 'Inventory';
const SHEET_PRODUCTS = 'Products';
const SHEET_CONFIG = 'Config';

// --- Column Indices (0-based) ---
// Transactions
const TX_TIMESTAMP = 0;
const TX_USER = 1;
const TX_TYPE = 2;
const TX_ITEM = 3;
const TX_QUANTITY = 4;
const TX_AMOUNT = 5;
const TX_ID = 6;
const TX_REF_ID = 7;

// Users
const USR_NAME = 0;
const USR_ACTIVE = 1;
const USR_BALANCE = 2;

// Inventory
const INV_NAME = 0;
const INV_UNIT_SIZE = 1;
const INV_COST = 2;
const INV_CRITICAL_THRESHOLD = 3;
const INV_LOW_THRESHOLD = 4;
const INV_STOCK = 5;

// Products
const PROD_NAME = 0;
const PROD_RECIPE = 1;
const PROD_CALC_PRICE = 2;
const PROD_PRICE = 3;

// =============================================================================
// Initialization
// =============================================================================

function initializeSheets() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();

    // Transactions
    let sheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    if (!sheet) {
        sheet = ss.insertSheet(SHEET_TRANSACTIONS);
        sheet.appendRow([
            'Timestamp', 'User', 'Type', 'Item', 'Quantity',
            'Amount (€)', 'Transaction ID', 'Ref Transaction ID'
        ]);
        sheet.setFrozenRows(1);
    }

    // Users
    sheet = ss.getSheetByName(SHEET_USERS);
    if (!sheet) {
        sheet = ss.insertSheet(SHEET_USERS);
        sheet.appendRow(['Name', 'Active', 'Balance (€)']);
        sheet.setFrozenRows(1);
    }

    // Inventory
    sheet = ss.getSheetByName(SHEET_INVENTORY);
    if (!sheet) {
        sheet = ss.insertSheet(SHEET_INVENTORY);
        sheet.appendRow([
            'Item Name', 'Unit Size (kg/L)', 'Cost per Unit (€)',
            'Critical Threshold', 'Low Threshold', 'Stock (units)'
        ]);
        sheet.setFrozenRows(1);
    }

    // Products
    sheet = ss.getSheetByName(SHEET_PRODUCTS);
    if (!sheet) {
        sheet = ss.insertSheet(SHEET_PRODUCTS);
        sheet.appendRow(['Item Name', 'Recipe', 'Calculated Price (€)', 'Price (€)']);
        sheet.setFrozenRows(1);
    }

    // Config
    sheet = ss.getSheetByName(SHEET_CONFIG);
    if (!sheet) {
        sheet = ss.insertSheet(SHEET_CONFIG);
        sheet.appendRow(['Key', 'Value']);
        sheet.setFrozenRows(1);
        sheet.appendRow(['treasurer', '']);
        sheet.appendRow(['paypal_link', '']);
        sheet.appendRow(['balance_threshold', '-10.00']);
        sheet.appendRow(['telegram_bot_token', '']);
        sheet.appendRow(['telegram_chat_id', '']);
        sheet.appendRow(['daily_report_hour', '8']);
    }
}

function addSampleData() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();

    // Sample users
    const users = ss.getSheetByName(SHEET_USERS);
    users.appendRow(['Christoph', true, 0]);
    users.appendRow(['Laura', true, 0]);
    users.appendRow(['Jonathan', true, 0]);

    // Sample inventory (Unit Size in kg/L)
    const inv = ss.getSheetByName(SHEET_INVENTORY);
    inv.appendRow(['Beans', 1, 30.00, 0.5, 1.5, 3.0]);
    inv.appendRow(['Water', 2, 0.85, 1.0, 3.0, 5.0]);
    inv.appendRow(['Cow Milk Carton', 1, 1.69, 1.0, 3.0, 5.0]);
    inv.appendRow(['Oat Milk Carton', 1, 2.55, 1.0, 3.0, 5.0]);

    // Sample products (recipe in unit fractions)
    const products = ss.getSheetByName(SHEET_PRODUCTS);
    products.appendRow(['Single', 'Beans:0.008,Water:0.015', 0.27, 0.30]);
    products.appendRow(['Double', 'Beans:0.016,Water:0.030', 0.54, 0.60]);
    products.appendRow(['Cow Milk Portion', 'Cow Milk Carton:0.100', 0.17, 0.20]);
    products.appendRow(['Oat Milk Portion', 'Oat Milk Carton:0.100', 0.26, 0.30]);

    // Config
    const config = ss.getSheetByName(SHEET_CONFIG);
    const configData = config.getDataRange().getValues();
    for (let i = 1; i < configData.length; i++) {
        if (configData[i][0] === 'treasurer') config.getRange(i + 1, 2).setValue('Christoph');
        if (configData[i][0] === 'paypal_link') config.getRange(i + 1, 2).setValue('paypal.me/crsknr');
        if (configData[i][0] === 'balance_threshold') config.getRange(i + 1, 2).setValue(-10.00);
    }
}

// =============================================================================
// Triggers
// =============================================================================

/**
 * Run this function ONCE to set up the daily inventory report trigger.
 */
function createDailyTrigger() {
    const triggers = ScriptApp.getProjectTriggers();
    for (const trigger of triggers) {
        if (trigger.getHandlerFunction() === 'sendDailyInventoryReport') {
            ScriptApp.deleteTrigger(trigger);
        }
    }

    const config = getConfigMap();
    const hour = parseInt(config['daily_report_hour']) || 8;

    ScriptApp.newTrigger('sendDailyInventoryReport')
        .timeBased()
        .atHour(hour)
        .everyDays(1)
        .create();
}

/**
 * Daily inventory report — called by time-based trigger.
 * Sends a Telegram message only if any items are below threshold.
 */
function sendDailyInventoryReport() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const invSheet = ss.getSheetByName(SHEET_INVENTORY);
    const invData = invSheet.getDataRange().getValues();

    const criticalItems = [];
    const lowItems = [];

    for (let i = 1; i < invData.length; i++) {
        const name = invData[i][INV_NAME];
        const unitSize = invData[i][INV_UNIT_SIZE];
        const stock = invData[i][INV_STOCK];
        const critical = invData[i][INV_CRITICAL_THRESHOLD];
        const low = invData[i][INV_LOW_THRESHOLD];
        const displayName = formatItemDisplayName(name);
        const displayStock = formatStockDisplay(stock, unitSize, name);
        const displayCritical = formatStockDisplay(critical, unitSize, name);
        const displayLow = formatStockDisplay(low, unitSize, name);

        if (stock < critical) {
            criticalItems.push(
                '🔴 ' + displayName + ': ' + displayStock +
                ' (critical — below ' + displayCritical + ')'
            );
        } else if (stock < low) {
            lowItems.push(
                '⚠️ ' + displayName + ': ' + displayStock +
                ' (low — below ' + displayLow + ')'
            );
        }
    }

    if (criticalItems.length === 0 && lowItems.length === 0) {
        return;
    }

    let message = '☕ Daily Inventory Report\n\n';
    if (criticalItems.length > 0) {
        message += criticalItems.join('\n') + '\n';
    }
    if (lowItems.length > 0) {
        message += lowItems.join('\n') + '\n';
    }
    message += '\nEverything else OK.';

    sendTelegram(message);
}

// =============================================================================
// HTTP Handlers
// =============================================================================

function doGet(e) {
    try {
        const action = e.parameter.action;

        switch (action) {
            case 'getConfig':
                return jsonResponse(handleGetConfig());
            case 'getUsers':
                return jsonResponse(handleGetUsers());
            case 'getInventory':
                return jsonResponse(handleGetInventory());
            case 'getLastTransactions':
                return jsonResponse(handleGetLastTransactions(
                    e.parameter.user,
                    parseInt(e.parameter.limit) || 10
                ));
            default:
                return jsonResponse({ status: 'error', error: 'Unknown action: ' + action });
        }
    } catch (err) {
        return jsonResponse({ status: 'error', error: err.message });
    }
}

function doPost(e) {
    try {
        const data = JSON.parse(e.postData.contents);

        if (!data.user) return jsonResponse({ status: 'error', error: 'Missing field: user' });
        if (!data.type) return jsonResponse({ status: 'error', error: 'Missing field: type' });
        if (!data.transactionId) return jsonResponse({ status: 'error', error: 'Missing field: transactionId' });

        if (isDuplicate(data.transactionId)) {
            const balance = getUserBalance(data.user);
            const criticalInventoryItem = getCriticalInventoryItem();
            return jsonResponse({ status: 'duplicate', balance: balance, criticalInventoryItem: criticalInventoryItem });
        }

        if (!isActiveUser(data.user)) {
            return jsonResponse({ status: 'error', error: 'Unknown user: ' + data.user });
        }

        switch (data.type) {
            case 'consume':
                return jsonResponse(handleConsume(data));
            case 'purchase':
                return jsonResponse(handlePurchase(data));
            case 'settle':
                return jsonResponse(handleSettle(data));
            case 'undo':
                return jsonResponse(handleUndo(data));
            default:
                return jsonResponse({ status: 'error', error: 'Unknown type: ' + data.type });
        }
    } catch (err) {
        return jsonResponse({ status: 'error', error: err.message });
    }
}

function jsonResponse(data) {
    return ContentService
        .createTextOutput(JSON.stringify(data))
        .setMimeType(ContentService.MimeType.JSON);
}

// =============================================================================
// GET Handlers
// =============================================================================

function handleGetConfig() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const config = getConfigMap();
    const treasurer = config['treasurer'] || '';

    // Users
    const usersSheet = ss.getSheetByName(SHEET_USERS);
    const usersData = usersSheet.getDataRange().getValues();
    const users = [];
    for (let i = 1; i < usersData.length; i++) {
        if (usersData[i][USR_ACTIVE]) {
            users.push({
                name: usersData[i][USR_NAME],
                balance: round2(usersData[i][USR_BALANCE]),
                isTreasurer: usersData[i][USR_NAME] === treasurer
            });
        }
    }
    users.sort((a, b) => a.name.localeCompare(b.name));

    // Products
    const productsSheet = ss.getSheetByName(SHEET_PRODUCTS);
    const productsData = productsSheet.getDataRange().getValues();
    const products = [];
    for (let i = 1; i < productsData.length; i++) {
        products.push({
            name: productsData[i][PROD_NAME],
            price: formatEuro(productsData[i][PROD_PRICE])
        });
    }

    // Inventory
    const inventory = buildInventoryResponse();

    return {
        status: 'success',
        users: users,
        products: products,
        inventory: inventory,
        config: {
            treasurer: treasurer,
            paypalLink: config['paypal_link'] || '',
            balanceThreshold: parseFloat(config['balance_threshold']) || -10.00,
            criticalInventoryItem: getCriticalInventoryItem()
        }
    };
}

function handleGetUsers() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const config = getConfigMap();
    const treasurer = config['treasurer'] || '';
    const usersSheet = ss.getSheetByName(SHEET_USERS);
    const usersData = usersSheet.getDataRange().getValues();
    const users = [];

    for (let i = 1; i < usersData.length; i++) {
        if (usersData[i][USR_ACTIVE]) {
            users.push({
                name: usersData[i][USR_NAME],
                balance: round2(usersData[i][USR_BALANCE]),
                isTreasurer: usersData[i][USR_NAME] === treasurer
            });
        }
    }
    users.sort((a, b) => a.name.localeCompare(b.name));

    return {
        status: 'success',
        users: users,
        criticalInventoryItem: getCriticalInventoryItem()
    };
}

function handleGetInventory() {
    return {
        status: 'success',
        inventory: buildInventoryResponse()
    };
}

function handleGetLastTransactions(user, limit) {
    if (!user) return { status: 'error', error: 'Missing parameter: user' };

    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const txSheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    const txData = txSheet.getDataRange().getValues();

    // Filter transactions for this user, newest first
    const userTx = [];
    for (let i = txData.length - 1; i >= 1; i--) {
        if (txData[i][TX_USER] === user) {
            userTx.push(txData[i]);
        }
    }

    // Expand multi-item transactions into display rows
    const displayRows = [];
    for (let t = 0; t < userTx.length && displayRows.length < limit; t++) {
        const tx = userTx[t];
        const items = String(tx[TX_ITEM]).split(',');
        const quantities = String(tx[TX_QUANTITY]).split(',');
        const timestamp = formatTimestamp(tx[TX_TIMESTAMP]);
        const action = formatAction(tx[TX_TYPE]);
        const amount = formatAmount(tx[TX_AMOUNT]);

        for (let j = 0; j < items.length && displayRows.length < limit; j++) {
            if (j === 0) {
                displayRows.push({
                    timestamp: timestamp,
                    action: action,
                    item: formatItemDisplayName(items[j].trim()),
                    qty: quantities[j] ? quantities[j].trim() : '',
                    amount: amount
                });
            } else {
                displayRows.push({
                    timestamp: '',
                    action: '',
                    item: formatItemDisplayName(items[j].trim()),
                    qty: quantities[j] ? quantities[j].trim() : '',
                    amount: ''
                });
            }
        }
    }

    return {
        status: 'success',
        transactions: displayRows
    };
}

// =============================================================================
// POST Handlers
// =============================================================================

function handleConsume(data) {
    if (!data.items || !data.quantities) {
        return { status: 'error', error: 'Missing field: items or quantities' };
    }
    if (data.items.length !== data.quantities.length) {
        return { status: 'error', error: 'Items and quantities length mismatch' };
    }

    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const productsMap = getProductsMap();

    // Calculate total and validate items
    let totalAmount = 0;
    for (let i = 0; i < data.items.length; i++) {
        const itemName = data.items[i];
        if (!productsMap[itemName]) {
            return { status: 'error', error: 'Unknown item: ' + itemName };
        }
        totalAmount += data.quantities[i] * productsMap[itemName].price;
    }
    totalAmount = round2(-totalAmount); // Negative for consume

    // Deduct inventory
    for (let i = 0; i < data.items.length; i++) {
        const recipe = productsMap[data.items[i]].recipe;
        deductInventory(recipe, data.quantities[i]);
    }

    // Write transaction
    const txSheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    txSheet.appendRow([
        new Date(),
        data.user,
        'consume',
        data.items.join(','),
        data.quantities.join(','),
        totalAmount,
        data.transactionId,
        ''
    ]);

    // Update user balance
    updateUserBalance(data.user, totalAmount);

    const balance = getUserBalance(data.user);
    return {
        status: 'success',
        balance: balance,
        criticalInventoryItem: getCriticalInventoryItem()
    };
}

function handlePurchase(data) {
    if (!data.items || !data.quantities) {
        return { status: 'error', error: 'Missing field: items or quantities' };
    }
    if (data.items.length !== data.quantities.length) {
        return { status: 'error', error: 'Items and quantities length mismatch' };
    }

    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const invMap = getInventoryMap();

    // Calculate total and validate items
    let totalAmount = 0;
    for (let i = 0; i < data.items.length; i++) {
        const itemName = data.items[i];
        if (!invMap[itemName]) {
            return { status: 'error', error: 'Unknown inventory item: ' + itemName };
        }
        totalAmount += data.quantities[i] * invMap[itemName].cost;
    }
    totalAmount = round2(totalAmount); // Positive for purchase

    // Add to inventory
    const invSheet = ss.getSheetByName(SHEET_INVENTORY);
    const invData = invSheet.getDataRange().getValues();
    for (let i = 0; i < data.items.length; i++) {
        for (let r = 1; r < invData.length; r++) {
            if (invData[r][INV_NAME] === data.items[i]) {
                const newStock = invData[r][INV_STOCK] + data.quantities[i];
                invSheet.getRange(r + 1, INV_STOCK + 1).setValue(round2(newStock));
                invData[r][INV_STOCK] = newStock;
                break;
            }
        }
    }

    // Write transaction
    const txSheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    txSheet.appendRow([
        new Date(),
        data.user,
        'purchase',
        data.items.join(','),
        data.quantities.join(','),
        totalAmount,
        data.transactionId,
        ''
    ]);

    // Update user balance
    updateUserBalance(data.user, totalAmount);

    const balance = getUserBalance(data.user);
    return {
        status: 'success',
        balance: balance,
        criticalInventoryItem: getCriticalInventoryItem()
    };
}

function handleSettle(data) {
    if (data.amount === undefined || data.amount === null) {
        return { status: 'error', error: 'Missing field: amount' };
    }

    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const amount = round2(Math.abs(data.amount)); // Always positive

    // Write transaction
    const txSheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    txSheet.appendRow([
        new Date(),
        data.user,
        'settle',
        'PayPal',
        '1',
        amount,
        data.transactionId,
        ''
    ]);

    // Update user balance
    updateUserBalance(data.user, amount);

    // Deduct from treasurer balance (they received the payment)
    const config = getConfigMap();
    const treasurer = config['treasurer'] || '';
    if (treasurer && treasurer !== data.user) {
        updateUserBalance(treasurer, -amount);
    }

    // Send Telegram notification
    sendTelegram('💰 ' + data.user + ' settled €' + amount.toFixed(2) + ' — check PayPal');

    const balance = getUserBalance(data.user);
    return {
        status: 'success',
        balance: balance,
        criticalInventoryItem: getCriticalInventoryItem()
    };
}

function handleUndo(data) {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const txSheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    const txData = txSheet.getDataRange().getValues();

    // Find the most recent non-undo transaction for this user
    let targetTx = null;
    for (let i = txData.length - 1; i >= 1; i--) {
        if (txData[i][TX_USER] === data.user) {
            if (txData[i][TX_TYPE] === 'undo') {
                return { status: 'error', error: 'Last transaction is already an undo' };
            }
            targetTx = txData[i];
            break;
        }
    }

    if (!targetTx) {
        return { status: 'error', error: 'Nothing to undo' };
    }

    const originalType = targetTx[TX_TYPE];
    const originalItems = String(targetTx[TX_ITEM]);
    const originalQuantities = String(targetTx[TX_QUANTITY]);
    const originalAmount = targetTx[TX_AMOUNT];
    const originalTxId = targetTx[TX_ID];

    // Negate quantities
    const quantities = originalQuantities.split(',').map(function (q) {
        const num = parseFloat(q.trim());
        return String(-num);
    }).join(',');

    // Reverse amount
    const reversedAmount = round2(-originalAmount);

    // Reverse inventory changes
    if (originalType === 'consume') {
        // Original consumed items — add them back via negative deduction
        const productsMap = getProductsMap();
        const itemNames = originalItems.split(',');
        const origQty = originalQuantities.split(',');
        for (let i = 0; i < itemNames.length; i++) {
            const itemName = itemNames[i].trim();
            if (productsMap[itemName]) {
                deductInventory(productsMap[itemName].recipe, -parseFloat(origQty[i].trim()));
            }
        }
    } else if (originalType === 'purchase') {
        // Original added stock — remove it
        const invSheet = ss.getSheetByName(SHEET_INVENTORY);
        const invData = invSheet.getDataRange().getValues();
        const itemNames = originalItems.split(',');
        const origQty = originalQuantities.split(',');
        for (let i = 0; i < itemNames.length; i++) {
            const itemName = itemNames[i].trim();
            for (let r = 1; r < invData.length; r++) {
                if (invData[r][INV_NAME] === itemName) {
                    const newStock = invData[r][INV_STOCK] - parseFloat(origQty[i].trim());
                    invSheet.getRange(r + 1, INV_STOCK + 1).setValue(round2(Math.max(0, newStock)));
                    invData[r][INV_STOCK] = newStock;
                    break;
                }
            }
        }
    }
    // settle: no inventory to reverse

    // Write undo transaction
    txSheet.appendRow([
        new Date(),
        data.user,
        'undo',
        originalItems,
        quantities,
        reversedAmount,
        data.transactionId,
        originalTxId
    ]);

    // Update user balance
    updateUserBalance(data.user, reversedAmount);

    // If undoing a settlement, reverse treasurer balance and notify
    if (originalType === 'settle') {
        const config = getConfigMap();
        const treasurer = config['treasurer'] || '';
        if (treasurer && treasurer !== data.user) {
            updateUserBalance(treasurer, -reversedAmount);
        }
        sendTelegram('↩️ ' + data.user + ' undid settlement of €' + Math.abs(originalAmount).toFixed(2));
    }

    const balance = getUserBalance(data.user);
    return {
        status: 'success',
        balance: balance,
        criticalInventoryItem: getCriticalInventoryItem()
    };
}

// =============================================================================
// Helper Functions
// =============================================================================

function getConfigMap() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const configSheet = ss.getSheetByName(SHEET_CONFIG);
    const data = configSheet.getDataRange().getValues();
    const map = {};
    for (let i = 1; i < data.length; i++) {
        map[data[i][0]] = data[i][1];
    }
    return map;
}

function getProductsMap() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_PRODUCTS);
    const data = sheet.getDataRange().getValues();
    const map = {};
    for (let i = 1; i < data.length; i++) {
        map[data[i][PROD_NAME]] = {
            recipe: data[i][PROD_RECIPE],
            price: data[i][PROD_PRICE]
        };
    }
    return map;
}

function getInventoryMap() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_INVENTORY);
    const data = sheet.getDataRange().getValues();
    const map = {};
    for (let i = 1; i < data.length; i++) {
        map[data[i][INV_NAME]] = {
            unitSize: data[i][INV_UNIT_SIZE],
            cost: data[i][INV_COST],
            criticalThreshold: data[i][INV_CRITICAL_THRESHOLD],
            lowThreshold: data[i][INV_LOW_THRESHOLD],
            stock: data[i][INV_STOCK]
        };
    }
    return map;
}

function isActiveUser(username) {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_USERS);
    const data = sheet.getDataRange().getValues();
    for (let i = 1; i < data.length; i++) {
        if (data[i][USR_NAME] === username && data[i][USR_ACTIVE]) {
            return true;
        }
    }
    return false;
}

function isDuplicate(transactionId) {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_TRANSACTIONS);
    const data = sheet.getDataRange().getValues();
    for (let i = data.length - 1; i >= 1; i--) {
        if (data[i][TX_ID] === transactionId) {
            return true;
        }
    }
    return false;
}

function getUserBalance(username) {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_USERS);
    const data = sheet.getDataRange().getValues();
    for (let i = 1; i < data.length; i++) {
        if (data[i][USR_NAME] === username) {
            return round2(data[i][USR_BALANCE]);
        }
    }
    return 0;
}

/**
 * Incrementally updates user balance.
 * Balance = Balance + amount (amount is already signed correctly)
 */
function updateUserBalance(username, amount) {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const sheet = ss.getSheetByName(SHEET_USERS);
    const data = sheet.getDataRange().getValues();

    for (let i = 1; i < data.length; i++) {
        if (data[i][USR_NAME] === username) {
            const newBalance = round2(data[i][USR_BALANCE] + amount);
            sheet.getRange(i + 1, USR_BALANCE + 1).setValue(newBalance);
            break;
        }
    }
}

/**
 * Deducts inventory based on a recipe string.
 * Recipe format: "Beans:0.016,Water:0.030" (values are in units, e.g. 0.016kg)
 * Pass negative quantity to add stock back (for undo).
 */
function deductInventory(recipe, quantity) {
    if (!recipe) return;

    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const invSheet = ss.getSheetByName(SHEET_INVENTORY);
    const invData = invSheet.getDataRange().getValues();

    // Parse recipe: "Beans:0.016,Water:0.030"
    const ingredients = recipe.split(',');
    for (const ingredient of ingredients) {
        const parts = ingredient.trim().split(':');
        if (parts.length !== 2) continue;

        const invItemName = parts[0].trim();
        const unitsPerItem = parseFloat(parts[1].trim());

        for (let r = 1; r < invData.length; r++) {
            if (invData[r][INV_NAME] === invItemName) {
                const unitsToDeduct = unitsPerItem * quantity;
                const newStock = invData[r][INV_STOCK] - unitsToDeduct;
                invSheet.getRange(r + 1, INV_STOCK + 1).setValue(round2(Math.max(0, newStock)));
                invData[r][INV_STOCK] = newStock; // Update local copy for multi-item
                break;
            }
        }
    }
}

/**
 * Builds the inventory array for API responses.
 * Returns pre-formatted strings for direct display on the Nextion.
 */
function buildInventoryResponse() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const invSheet = ss.getSheetByName(SHEET_INVENTORY);
    const invData = invSheet.getDataRange().getValues();
    const inventory = [];

    for (let i = 1; i < invData.length; i++) {
        const name = invData[i][INV_NAME];
        const unitSize = invData[i][INV_UNIT_SIZE];
        const cost = invData[i][INV_COST];
        const stock = invData[i][INV_STOCK];
        const critical = invData[i][INV_CRITICAL_THRESHOLD];
        const low = invData[i][INV_LOW_THRESHOLD];

        inventory.push({
            item: name,
            price: formatInventoryPrice(cost, unitSize, name),
            stock: formatStockDisplay(stock, unitSize, name),
            status: getStockStatus(stock, critical, low)
        });
    }

    return inventory;
}

/**
 * Returns display-ready string for critical inventory items only.
 * Only items below the CRITICAL threshold are shown on the display.
 */
function getCriticalInventoryItem() {
    const ss = SpreadsheetApp.getActiveSpreadsheet();
    const invSheet = ss.getSheetByName(SHEET_INVENTORY);
    const invData = invSheet.getDataRange().getValues();

    let worstItem = null;
    let worstRatio = Infinity;

    for (let i = 1; i < invData.length; i++) {
        const stock = invData[i][INV_STOCK];
        const critical = invData[i][INV_CRITICAL_THRESHOLD];

        if (stock < critical) {
            const ratio = critical > 0 ? stock / critical : 0;
            if (ratio < worstRatio) {
                worstItem = invData[i][INV_NAME];
                worstRatio = ratio;
            }
        }
    }

    if (!worstItem) return '';
    return 'Low on ' + formatItemDisplayName(worstItem) + '!';
}

function getStockStatus(stock, criticalThreshold, lowThreshold) {
    if (stock < criticalThreshold) return 'red';
    if (stock < lowThreshold) return 'yellow';
    return 'green';
}

// =============================================================================
// Telegram
// =============================================================================

/**
 * Sends a message via Telegram bot.
 * Silently returns if bot token or chat ID are not configured.
 */
function sendTelegram(message) {
    const config = getConfigMap();
    const token = config['telegram_bot_token'];
    const chatId = config['telegram_chat_id'];

    if (!token || !chatId) return;

    try {
        UrlFetchApp.fetch('https://api.telegram.org/bot' + token + '/sendMessage', {
            method: 'post',
            contentType: 'application/json',
            payload: JSON.stringify({
                chat_id: chatId,
                text: message,
                parse_mode: 'HTML'
            }),
            muteHttpExceptions: true
        });
    } catch (err) {
        console.error('Telegram notification failed: ' + err.message);
    }
}

// =============================================================================
// Formatting Helpers
// =============================================================================

/**
 * Formats inventory item names to friendly display names.
 */
function formatItemDisplayName(itemName) {
    const map = {
        'Beans': 'Beans',
        'Water': 'Water',
        'Cow Milk Carton': 'Cow Milk',
        'Oat Milk Carton': 'Oat Milk',
        'Cow Milk Portion': 'Cow Milk',
        'Oat Milk Portion': 'Oat Milk'
    };
    return map[itemName] || itemName;
}

/**
 * Formats stock for display: "1.350kg", "4.670L"
 * Beans use kg, everything else uses L.
 */
function formatStockDisplay(units, unitSize, itemName) {
    const totalSize = units * unitSize;
    const unit = itemName === 'Beans' ? 'kg' : 'L';
    return totalSize.toFixed(3) + unit;
}

/**
 * Formats inventory price for display: "30€ / 1kg", "0.85€ / 2L"
 */
function formatInventoryPrice(cost, unitSize, itemName) {
    const unit = itemName === 'Beans' ? 'kg' : 'L';
    const costStr = cost % 1 === 0 ? cost.toFixed(0) : cost.toFixed(2);
    const sizeStr = unitSize % 1 === 0 ? unitSize.toFixed(0) : unitSize.toFixed(1);
    return costStr + '€ / ' + sizeStr + unit;
}

/**
 * Formats a price with euro sign: "0.30€", "30€"
 */
function formatEuro(price) {
    if (price % 1 === 0) {
        return price.toFixed(0) + '€';
    }
    return price.toFixed(2) + '€';
}

function formatTimestamp(date) {
    if (!(date instanceof Date)) {
        date = new Date(date);
    }
    const day = String(date.getDate()).padStart(2, '0');
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return day + '.' + month + ' | ' + hours + ':' + minutes;
}

function formatAction(type) {
    const map = {
        'consume': 'Consume',
        'purchase': 'Purchase',
        'settle': 'Settling',
        'undo': 'Undo'
    };
    return map[type] || type;
}

function formatAmount(amount) {
    const num = parseFloat(amount);
    if (isNaN(num)) return '';
    const sign = num >= 0 ? '+' : '';
    return sign + num.toFixed(2);
}

function round2(num) {
    return Math.round((num + Number.EPSILON) * 100) / 100;
}