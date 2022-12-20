/** A quick test by feeding an recorded audio file. */

const { RealtimeSttWhisper } = require('bindings')('addon')
const fs = require('fs')
const path = require('path')

// Test audio file has to be converted with
// ffmpeg -i jfk.wav -ar 16000 -ac 1 -c:a pcm_f32le -f f32le jfk.raw
const kTestAudioFile = path.join(__dirname, 'whisper.cpp/samples/jfk.raw')
const kModelFile = path.join(__dirname, 'whisper.cpp/models/ggml-tiny.en.bin')
const kBytesPerSample = 4

const rawAudioBuffer = fs.readFileSync(kTestAudioFile) // Uint8 buffer
const nSamples = rawAudioBuffer.length / kBytesPerSample
const pcmf32 = new Float32Array(nSamples) // array of 32-bit float

// Fill in pcmf32 with proper sample values.
for (
  let bOffset = 0;
  bOffset < rawAudioBuffer.length - kBytesPerSample + 1;
  bOffset += kBytesPerSample
) {
  const sample = rawAudioBuffer.readFloatLE(bOffset)
  const iSample = bOffset / kBytesPerSample
  pcmf32[iSample] = sample
  if (iSample < 10) {
    console.log(`sample ${iSample}: ${sample}${iSample === 9 ? '\n' : ''}`)
  }
}

// Init stt engine
const stt = new RealtimeSttWhisper(kModelFile)

// Poll transcribed data
const interval = setInterval(() => {
  const transcribed = stt.getTranscribed()
  console.log('transcribed:', transcribed)
}, 500)

stt.addAudioData(pcmf32)
