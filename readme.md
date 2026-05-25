SERVER -> worker queue

worker
  ↓
router
  ↓
handler
  ↓
business logic
  ↓
database

server/
    sockets
    HTTP
    routing
    handlers
    threadpool
vending/
    buy_item()
    get_stock()
    restock()
db/
    SQL helpers
    prepared statements
    transactions
common/
    shared structs
    utils
    logging