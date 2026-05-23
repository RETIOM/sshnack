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
-- Row A: Carbonated Drinks (prices in groszy, e.g. 500 = 5.00 PLN)
('Cola Classic',           500),
('Diet Cola',              500),
('Zero Sugar Cola',        500),
('Sparkling Water',        400),
('Lemon Soda',             500),
('Cherry Cola',            550),
-- Row B: Non-Carbonated Drinks
('Spring Water',           350),
('Orange Juice',           650),
('Apple Juice',            600),
('Iced Tea',               500),
('Sports Drink',           650),
('Energy Drink',           800),
-- Row C: Chips & Crackers
('Potato Chips',           500),
('Spicy Nacho Doritos',    550),
('Pretzels',               400),
('Cheese Crackers',        400),
('Popcorn',                450),
('BBQ Chips',              500),
-- Row D: Candy & Sweets
('Chocolate Peanut Bar',   450),
('Gummy Bears',            400),
('M&Ms',                   550),
('Snickers',               500),
('Kit Kat',                450),
('Oreo Cookies',           550),
-- Row E: Healthy & Protein
('Mixed Nuts',             650),
('Protein Bar',           1000),
('Granola Bar',            650),
('Dried Fruit Mix',        600),
('Trail Mix',              650),
('Peanut Butter Crackers', 500);

INSERT INTO stock (slot_id, item_id, quantity) VALUES
-- Row A: Carbonated Drinks
(11, 1,  18),
(12, 2,  18),
(13, 3,  18),
(14, 4,  20),
(15, 5,  15),
(16, 6,  12),
-- Row B: Non-Carbonated Drinks
(21, 7,  24),
(22, 8,  12),
(23, 9,  12),
(24, 10, 15),
(25, 11, 10),
(26, 12, 10),
-- Row C: Chips & Crackers
(31, 13, 15),
(32, 14, 15),
(33, 15, 18),
(34, 16, 18),
(35, 17, 15),
(36, 18, 15),
-- Row D: Candy & Sweets
(41, 19, 20),
(42, 20, 20),
(43, 21, 18),
(44, 22, 15),
(45, 23, 18),
(46, 24, 18),
-- Row E: Healthy & Protein
(51, 25, 12),
(52, 26, 10),
(53, 27, 12),
(54, 28, 12),
(55, 29, 10),
(56, 30, 15);

INSERT INTO stock_logs (item_id, change_type, quantity_changed) VALUES
(1,  'addition', 18),
(2,  'addition', 18),
(3,  'addition', 18),
(4,  'addition', 20),
(5,  'addition', 15),
(6,  'addition', 12),
(7,  'addition', 24),
(8,  'addition', 12),
(9,  'addition', 12),
(10, 'addition', 15),
(11, 'addition', 10),
(12, 'addition', 10),
(13, 'addition', 15),
(14, 'addition', 15),
(15, 'addition', 18),
(16, 'addition', 18),
(17, 'addition', 15),
(18, 'addition', 15),
(19, 'addition', 20),
(20, 'addition', 20),
(21, 'addition', 18),
(22, 'addition', 15),
(23, 'addition', 18),
(24, 'addition', 18),
(25, 'addition', 12),
(26, 'addition', 10),
(27, 'addition', 12),
(28, 'addition', 12),
(29, 'addition', 10),
(30, 'addition', 15);

COMMIT;
