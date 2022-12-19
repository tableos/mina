const { RealtimeSttWhisper } = require("bindings")("addon");
const fs = require("fs");

const sampleSize = 4; // bytes
const buffer = fs.readFileSync("test.raw"); // f32le
const nSamples = buffer.length / sampleSize;
const float32Array = new Float32Array(nSamples);
console.log(nSamples);

for (let bOffset = 0; bOffset < buffer.length - sampleSize + 1; bOffset += sampleSize) {
  const sample = buffer.readFloatLE(bOffset);
  const iSample = bOffset / sampleSize;
  if (iSample < 10) console.log(sample);
  float32Array[iSample] = sample;
}

const stt = new RealtimeSttWhisper("ggml-model-whisper-tiny.en.bin");
const interval = setInterval(() => {
  const text = stt.getTranscribed();
  console.log("text:", text);
}, 500);

// const sampleRate = 16000;
// const hz = 1000;
// const samples = new Float32Array(sampleRate);
// for (let i = 0; i < samples.length; i++) {
//   samples[i] = Math.sin((i * Math.PI * 2) / (sampleRate / hz));
// }
stt.addAudioData(float32Array);
