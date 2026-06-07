const express = require('express');
const app = express();

app.get('/json', (req, res) => {
  res.json({ message: 'Hello World' });
});

app.get('/text', (req, res) => {
  res.send('Hello World');
});

app.listen(3000);
