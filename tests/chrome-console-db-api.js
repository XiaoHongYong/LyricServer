fetch('/db-api/user', {
  method: 'POST',
  body: JSON.stringify({
    action: 'prepare',
    sql: 'select * from users where UserName="henry_xiao"',
  })
})
.then(res => res.json())
.then(console.log)
// 返回 stmt-id

// 使用返回的 stmt-id 用于查询
fetch('/db-api/user', {
method: 'POST',
body: JSON.stringify({
    action: 'query',
    "stmt-id": "5FF6E89ACD4B38A8D21D828F3C28BB42",
    "col-names": true,
    "args": [],
})
})
.then(res => res.json())
.then(console.log)


////

fetch('/db-api/user', {
    method: 'POST',
    body: JSON.stringify({
    action: 'prepare',
    sql: 'select * from users where UserName=?',
    })
})
    .then(res => res.json())
    .then(console.log)

fetch('/db-api/user', {
method: 'POST',
body: JSON.stringify({
    action: 'query',
    "stmt-id": "EC6820A8316ACC7ECA5DD2D145B43CF0",
    "col-names": true,
    "args": ['henry_xiao'],
})
})
.then(res => res.json())
.then(console.log)

