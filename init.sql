PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

DROP TABLE IF EXISTS stock_logs;
DROP TABLE IF EXISTS transactions;
DROP TABLE IF EXISTS stock;
DROP TABLE IF EXISTS items;
DROP TABLE IF EXISTS users;

CREATE TABLE users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    balance INTEGER NOT NULL DEFAULT 0 CHECK (balance >= 0)
);

CREATE TABLE items (
    item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    price INTEGER NOT NULL
);

CREATE TABLE stock (
    slot_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL,
    quantity INTEGER NOT NULL CHECK (quantity >= 0),
    FOREIGN KEY (item_id) REFERENCES items(item_id)
);

CREATE TABLE transactions (
    transaction_id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL CHECK (type IN ('purchase', 'deposit', 'withdrawal', 'refund')),
    user_id INTEGER NOT NULL,
    item_id INTEGER,
    quantity INTEGER,
    total_amount INTEGER NOT NULL,
    transaction_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (item_id) REFERENCES items(item_id),
    CHECK (
        (type = 'purchase' AND item_id IS NOT NULL AND quantity > 0 AND total_amount < 0) OR
        (type = 'deposit' AND item_id IS NULL AND quantity IS NULL AND total_amount > 0) OR
        (type = 'withdrawal' AND item_id IS NULL AND quantity IS NULL AND total_amount < 0) OR
        (type = 'refund' AND item_id IS NOT NULL AND quantity > 0 AND total_amount > 0)
    )
);

CREATE TABLE stock_logs (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER NOT NULL,
    change_type TEXT NOT NULL CHECK (change_type IN ('addition', 'removal')),
    quantity_changed INTEGER NOT NULL,
    log_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (item_id) REFERENCES items(item_id),
    CHECK (
        (change_type = 'addition' AND quantity_changed > 0) OR
        (change_type = 'removal' AND quantity_changed < 0)
    )
);

COMMIT;

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

INSERT INTO items (name, price) VALUES
('Cola Classic', 150),
('Diet Cola', 150),
('Spring Water', 100),
('Orange Juice', 225),
('Potato Chips', 175),
('Spicy Nacho Doritos', 175),
('Chocolate Peanut Bar', 150),
('Gummy Bears', 125),
('Mixed Nuts', 200),
('Protein Bar', 250);

INSERT INTO stock (slot_id, item_id, quantity) VALUES
(11, 1, 15),
(12, 2, 15),
(13, 3, 20),
(14, 4, 10),
(15, 5, 12),
(21, 6, 12),
(22, 7, 24),
(23, 8, 20),
(24, 9, 10),
(25, 10, 15);

INSERT INTO stock_logs (item_id, change_type, quantity_changed) VALUES
(1, 'addition', 15),
(2, 'addition', 15),
(3, 'addition', 20),
(4, 'addition', 10),
(5, 'addition', 12),
(6, 'addition', 12),
(7, 'addition', 24),
(8, 'addition', 20),
(9, 'addition', 10),
(10, 'addition', 15);

COMMIT;
