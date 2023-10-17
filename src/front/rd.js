// Create express app
const express = require('express');
const app = express();
const configs = require('./configs.js').configs;
const fs = require('fs');
const https = require('https');
const cors = require('cors');
const path = require('path');
const sqlite3 = require('sqlite3').verbose();
const basicAuth = require('express-basic-auth');
const imagesDir = path.join(__dirname, '..', '..', 'images/');
const databasePath = path.join(__dirname, '..', '..', 'data.sqlite');


https.createServer(
    {
      key: fs.readFileSync(configs.ssl.key),
      cert: fs.readFileSync(configs.ssl.crt)
    },
    app
).listen(configs.port, function() {
  console.log(
      `rd.js listening on https://0.0.0.0:${configs.port}!`
  );
});

app.use(basicAuth({
  users: configs.users,
  challenge: true // <--- needed to actually show the login dialog!
}));

app.use(cors({
  origin: 'https://rpi-rack.sz.lan:4443'
}));

app.use('/', express.static(path.join(__dirname, 'public/')));
/*
app.get('/get_rack_door_states_json/', (req, res, next) => {
  const db = new sqlite3.Database(databasePath, (err) => {
    if (err) {
      res.status(500).json({
        'status': 'error',
        'message': 'Cannot open database'
      });
      console.error(err.message);
    } else {
      const sql = 'SELECT * FROM door_state ORDER BY record_time DESC LIMIT 6';
      const params = [];
      db.all(sql, params, (err, rows) => {
        if (err) {
          res.status(500).json({
            'status': 'error',
            'message': err.message
          });
          return;
        }
        res.json({
          'status': 'success',
          'data': rows
        });
      });
    }
  });
  db.close();
});
*/
app.get('/get_images_list_json/', (req, res, next) => {
  const imagesNameList = [];
  fs.readdir(imagesDir, (err, fileNames) => {
    // According to this link: https://github.com/nodejs/node/issues/3232
    // On Linux, fs.readdir()'s result is guaranteed to be sorted.
    // However, this behavior is not documented.
    fileNames.sort().reverse();
    fileNames = fileNames.slice(0, 72);
    fileNames.reverse();
    if (err) {
      res.status(500).json({
        'status': 'error',
        'message': 'Unable to scan imagesDir'
      });
    }
    fileNames.forEach((imageName) => {
      imagesNameList.push(imageName);
    });
    res.json({
      'status': 'success',
      'data': imagesNameList
    });
  });
});

app.get('/get_images_jpg/', (req, res, next) => {
  const imageName = req.query.imageName;
  if (typeof imageName !== 'string') {
    res.status(400).json({
      'status': 'error',
      'message': 'parameter imageName is either undefined or not in proper format'
    });
  }
  res.sendFile(imagesDir + imageName);
  // seems that sendFile() has built-in root directory traversal detection function.
});

app.use('/', (req, res) => {
  return res.redirect('/html/index.html');
});

app.get('*', function(req, res) {
  res.status(404).send('This is a naive 404 page--the URL you try to access does not exist!');
});
