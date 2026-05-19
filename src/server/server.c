// 1. accept connection
// 2. read raw HTTP request
// 3. enqueue request/socket
// 4. worker thread wakes up
// 5. parse HTTP
// 6. route request
// 7. call business logic
// 8. send response
// 9. close connection

