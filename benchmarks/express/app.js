const cluster = require('cluster');
const os = require('os');
const express = require('express');

if (cluster.isPrimary) {
  const numCPUs = os.cpus().length;
  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }
} else {
  const app = express();

  app.get('/json', (req, res) => {
    res.json({ message: 'Hello World' });
  });

  app.get('/text', (req, res) => {
    res.send('Hello World');
  });

  app.listen(3000);
}
