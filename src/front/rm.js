// Create express app
const express = require('express');
const app = express();
const configs = require('./configs.js').configs;
const fs = require('fs');
const https = require('https');
const path = require('path');
const sqlite3 = require('sqlite3').verbose();

const imagesDir = path.join(__dirname, '..', '..', 'images/');


https.createServer(
    {
      key: fs.readFileSync(configs.ssl.key),
      cert: fs.readFileSync(configs.ssl.crt)
    },
    app
).listen(configs.port, function() {
  console.log(
      `Example app listening on port ${configs.port}!`
  );
});

// Root endpoint
app.get('/', (req, res, next) => {
  res.json({'message': 'Ok'});
});

app.get('/get_temp_control_json/', (req, res, next) => {
  const dbPath = '../../data.sqlite';
  const db = new sqlite3.Database(dbPath, (err) => {
    if (err) {
      res.status(500).json({
        'status': 'error',
        'message': 'Cannot open database'
      });
      console.error(err.message);
    } else {
      const sql = 'SELECT * FROM temp_control ORDER BY record_time DESC LIMIT 1';
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

app.get('/get_rack_door_states_json/', (req, res, next) => {
  const dbPath = '../../data.sqlite';
  const db = new sqlite3.Database(dbPath, (err) => {
    if (err) {
      res.status(500).json({
        'status': 'error',
        'message': 'Cannot open database'
      });
      console.error(err.message);
    } else {
      const sql = 'SELECT * FROM door_state ORDER BY record_time DESC LIMIT 5';
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

app.get('/get_images_list_json/', (req, res, next) => {
  const imagesNameList = [];
  fs.readdir(imagesDir, (err, fileNames) => {
    // According to this link: https://github.com/nodejs/node/issues/3232
    // On Linux, fs.readdir()'s result is guaranteed to be sorted.
    // However, this behavior is not documented.
    fileNames.sort().reverse();
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
      'data': imagesNameList.slice(0, 24)
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

// Default response for any other request
app.use(function(req, res) {
  res.status(404);
});
