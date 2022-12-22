/**
 * References:
 * - https://github.com/guest271314/audioInputToWav/blob/master/index.html
 * - https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor#processing_audio
 */

/**
 * @typedef ProcessorOptions
 * @property {number} channelCount The number of channels to record.
 * @property {number} reportSize Report data every time this number of
 * samples are accumulated. Must be >= 128 and recommended to be the
 * multiple of 128 for zero latency.
 */

/**
 * @typedef Options
 * @property {ProcessorOptions} processorOptions
 */

/**
 * @class RecorderProcessor
 * @extends AudioWorkletProcessor
 *
 * A recorder that exposes raw audio data (PCM, f32) to the main thread via
 * message events. Useful for streaming applications that need to access
 * raw audio data.
 */
class RecorderProcessor extends AudioWorkletProcessor {
  /**
   * @param {Options} options
   */
  constructor(options) {
    super()

    this.enable = true

    const reportSize =
      options.processorOptions && options.processorOptions.reportSize
    if (!reportSize || reportSize < 128) {
      log('reportSize must be >= 128, will be set to 128')
      this.reportSize = 128
    } else {
      this.reportSize = reportSize
    }

    const recordChannelCount =
      options.processorOptions && options.processorOptions.channelCount
    if (!recordChannelCount || recordChannelCount < 1) {
      log('Must record at least 1 channel, will be set to one channel')
      this.recordChannelCount = 1
    } else {
      this.recordChannelCount = recordChannelCount
    }

    /** Create buffer for each channel. */
    /** @type {number[][]} */
    this.buffer = []
    for (let i = 0; i < this.recordChannelCount; i++) {
      this.buffer[i] = []
    }

    this.port.onmessage = (e) => {
      if (e.data === 'pause') this.enable = false
      else if (e.data === 'resume') this.enable = true
    }
  }

  process(inputs, outputs) {
    if (!this.enable) {
      this.buffer = this.buffer.map((_) => [])
      return true
    }

    const input = inputs[0]
    const output = outputs[0]
    if (!input || !input.length) {
      return true
    }

    for (
      let channel = 0;
      channel < Math.min(input.length, this.recordChannelCount);
      channel++
    ) {
      let inputChannel
      try {
        inputChannel = input[channel]
        this.buffer[channel].push(...inputChannel.slice())
      } catch (e) {
        error(e, { channel, inputs, outputs, input, output })
      }
    }
    
    if (this.buffer[0].length >= this.reportSize) {
      const recordBuffer = []
      for (let channel = 0; channel < this.buffer.length; channel++) {
        const floats = this.buffer[channel].slice(0, this.reportSize)
        recordBuffer[channel] = new Float32Array(floats)
        this.buffer[channel].splice(0, this.reportSize)
        // log(
        //   `channel ${channel}: report ${recordBuffer[channel].length}, remain ${this.buffer[channel].length}`
        // )
      }

      this.port.postMessage({ currentFrame, sampleRate, recordBuffer })
    }

    /**
     * Doc:
     * https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor/process#return_value
     * From experimenting, this should be `true` otherwise `process` is
     * only called once.
     */
    return true
  }
}

registerProcessor('recorder-processor', RecorderProcessor)

function log(...args) {
  console.log('recorderWorklet:', ...args)
}

function error(...args) {
  console.error('recorderWorklet:', ...args)
}
