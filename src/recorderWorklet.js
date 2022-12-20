/**
 * References:
 * - https://github.com/guest271314/audioInputToWav/blob/master/index.html
 * - https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor#processing_audio
 */

/**
 * @typedef ProcessorOptions
 * @property {number} channelCount A channel count to record.
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
    log(options)
    this._createdAt = currentTime
    this._elapsed = 0
    this._recordChannelCount =
      (options.processorOptions && options.processorOptions.channelCount) || 1
    this.enable = true
    this.port.onmessage = (e) => {
      if (e.data === 'pause') this.enable = false
      else if (e.data === 'resume') this.enable = true
    }
  }

  process(inputs, outputs) {
    // Records the incoming data from |inputs| and also bypasses the data to
    // |outputs|.
    const input = inputs[0]
    const output = outputs[0]
    const channelsData = [] // [0] -> channel 0, [1] -> channel 1, ...

    if (!input.length) {
      log('input length is 0:', input)
      return true
    }

    for (let channel = 0; channel < input.length; channel++) {
      let inputChannel, outputChannel
      try {
        inputChannel = input[channel]
        outputChannel = output[channel]
        // outputChannel.set(inputChannel);

        if (this.enable) channelsData[channel] = inputChannel.slice()
      } catch (e) {
        error(e, { channel, inputs, outputs, input, output })
      }
    }

    if (this.enable) {
      this.port.postMessage({
        currentFrame,
        sampleRate,
        recordBuffer: channelsData.map((floats) => new Float32Array(floats)),
      })
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
