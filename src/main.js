/** https://www.electronjs.org/docs/latest/tutorial/quick-start */

const { RealtimeSttWhisper } = require('bindings')('addon')
const { app, BrowserWindow, ipcMain } = require('electron')
const fs = require('fs')
const path = require('path')

const { CH_ADD_AUDIO_DATA, CH_GET_TRANSCRIBED } = require('./ipc')

const createWindow = () => {
  const win = new BrowserWindow({
    width: 1000,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
    },
  })

  win.loadFile('index.html')
  win.webContents.openDevTools()
}

app.whenReady().then(() => {
  const kModelFile = path.join(
    __dirname,
    '../whisper.cpp/models/ggml-base.en.bin'
  )
  if (!fs.existsSync(kModelFile)) {
    console.error('Cannot find whisper model file. Abort.')
    process.exit(1)
  }

  const stt = new RealtimeSttWhisper(kModelFile)

  ipcMain.handle(CH_ADD_AUDIO_DATA, (e, data) => {
    if (data instanceof Float32Array) {
      stt.addAudioData(data)
    } else {
      console.log('not Float32Array', data)
    }
  })

  ipcMain.handle(CH_GET_TRANSCRIBED, () => {
    return stt.getTranscribed()
  })

  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit()
})
