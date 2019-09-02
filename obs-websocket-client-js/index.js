const OBS = require('obs-websocket-js');

const client = new OBS();
client.connect({ address: 'localhost:4444' })
  .then(async () => {
    const newData = await client.send('SetRecordingFolder', { 'rec-folder': 'C:/videos' });
    return newData;
  })
  .then(async () => {
    const newData = await client.send('StartRecording'); // 녹화 시작 request
    return newData;
  })
  .then(async () => {
    const newData = await client.send('StopRecording');
    return newData;
  })
  .then(async () => { // 소스 리스트 얻기
    const newData = await client.send('GetSourcesList');
    return newData;
  })
  .then(console.log)
  .catch(console.error);
