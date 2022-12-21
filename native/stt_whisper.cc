#include "stt_whisper.h"
#include "whisper.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

void print_array(const std::vector<float>& data)
{
  fprintf(stdout, "print array: [");
  for (int i = 0; i < std::min((int)data.size(), 10); i++) {
    fprintf(stdout, " %.8f,", data[i]);
  }
  fprintf(stdout, " ]\n");
}

void high_pass_filter(std::vector<float>& data, float cutoff, float sample_rate)
{
  const float rc = 1.0f / (2.0f * M_PI * cutoff);
  const float dt = 1.0f / sample_rate;
  const float alpha = dt / (rc + dt);

  float y = data[0];

  for (size_t i = 1; i < data.size(); i++) {
    y = alpha * (y + data[i] - data[i - 1]);
    data[i] = y;
  }
}

/** Check if speech is ending. */
bool vad_simple(std::vector<float>& pcmf32, int sample_rate, int last_ms, float vad_thold, float freq_thold, bool verbose)
{
  const int n_samples = pcmf32.size();
  const int n_samples_last = (sample_rate * last_ms) / 1000;

  if (n_samples_last >= n_samples) {
    // not enough samples - assume no speech
    return false;
  }

  if (freq_thold > 0.0f) {
    high_pass_filter(pcmf32, freq_thold, sample_rate);
  }

  float energy_all = 0.0f;
  float energy_last = 0.0f;

  for (int i = 0; i < n_samples; i++) {
    energy_all += fabsf(pcmf32[i]);

    if (i >= n_samples - n_samples_last) {
      energy_last += fabsf(pcmf32[i]);
    }
  }

  energy_all /= n_samples;
  energy_last /= n_samples_last;

  if (verbose) {
    fprintf(stderr, "%s: energy_all: %f, energy_last: %f, vad_thold: %f, freq_thold: %f\n", __func__, energy_all, energy_last, vad_thold, freq_thold);
  }

  if ((energy_all < 0.0001f && energy_last < 0.0001f) || energy_last > vad_thold * energy_all) {
    return false;
  }

  return true;
}

RealtimeSttWhisper::RealtimeSttWhisper(const std::string& path_model)
{
  ctx = whisper_init(path_model.c_str());
  is_running = true;
  worker = std::thread(&RealtimeSttWhisper::Run, this);
  t_last_iter = std::chrono::high_resolution_clock::now();
}

RealtimeSttWhisper::~RealtimeSttWhisper()
{
  is_running = false;
  if (worker.joinable())
    worker.join();
  whisper_free(ctx);
}

/** Add audio data in PCM f32 format. */
void RealtimeSttWhisper::AddAudioData(const std::vector<float>& data)
{
  std::lock_guard<std::mutex> lock(s_mutex);
  // printf("AddAudioData: remaining: %d, new: %d\n", (int)s_queued_pcmf32.size(), (int)data.size());
  s_queued_pcmf32.insert(s_queued_pcmf32.end(), data.begin(), data.end());
}

/** Get newly transcribed text. */
std::vector<transcribed_msg> RealtimeSttWhisper::GetTranscribed()
{
  std::vector<transcribed_msg> transcribed;
  std::lock_guard<std::mutex> lock(s_mutex);
  transcribed = std::move(s_transcribed_msgs);
  s_transcribed_msgs.clear();
  return transcribed;
}

/** Run Whisper in its own thread to not block the main thread. */
void RealtimeSttWhisper::Run()
{
  struct whisper_full_params wparams = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);

  // See here for example https://github.com/ggerganov/whisper.cpp/blob/master/examples/stream/stream.cpp#L302
  wparams.n_threads = 4;
  wparams.no_context = true;
  wparams.single_segment = true;
  wparams.print_progress = false;
  wparams.print_timestamps = false;
  wparams.max_tokens = 64;
  wparams.language = "en";

  /* When more than this amount of audio received, run an iteration. Note
  that since whisper is currently designed to process audio in 30-second
  chunks even with smaller inputs, the . */
  const int trigger_ms = 400;
  /* When more than this amount of audio accumulates in current context,
  clear current context and enter a new iteration after this iteration.
  TODO: Replace with proper VAD (voice activity detection). */
  const int iter_threshold_ms = 16000;  // why iter usually stop at half?
  /* The design of trigger and threshold allows inputing audio at different
  rate without external config. Inspired by Assembly.ai
  (https://github.com/misraturp/Real-time-transcription-from-microphone/blob/main/speech_recognition.py)
  */

  const int n_samples_trigger = (trigger_ms / 1000.0) * WHISPER_SAMPLE_RATE;
  const int n_samples_iter_threshold = (iter_threshold_ms / 1000.0) * WHISPER_SAMPLE_RATE;

  /* VAD parameters */
  const int vad_window_s = 3;  // the last 3s
  const int n_samples_vad_window = WHISPER_SAMPLE_RATE * vad_window_s;
  const int vad_last_ms = 450;  // will compare the energy of the last 450ms to that of the total 3s
  const int n_samples_keep_iter = WHISPER_SAMPLE_RATE * 0.1;
  const float vad_thold = 0.25f;
  const float freq_thold = 200.0f;

  /* Audio buffer */
  std::vector<float> pcmf32;

  /* Processing loop */
  while (is_running) {
    {
      std::unique_lock<std::mutex> lock(s_mutex);

      if (s_queued_pcmf32.size() < n_samples_trigger) {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
    }

    {
      std::lock_guard<std::mutex> lock(s_mutex);

      if (s_queued_pcmf32.size() > 2 * n_samples_iter_threshold) {
        fprintf(stderr, "\n\n%s: WARNING: too much audio is going to be processed, result may not come out in real time\n\n", __func__);
      }
    }

    {
      std::lock_guard<std::mutex> lock(s_mutex);

      const int n_samples_new = s_queued_pcmf32.size();
      const int n_samples_old = pcmf32.size();
      // const int n_samples_from_old = std::min(
      //     n_samples_old /* cannot take more than exisitng */,
      //     WHISPER_SAMPLE_RATE /* 1 second */
      // );

      // pcmf32.resize(n_samples_old + n_samples_new);

      // for (int i = 0; i < n_samples_from_old; i++) {
      //   // never moves from smaller indexes to larger ones
      //   pcmf32[i] = pcmf32[n_samples_old - n_samples_from_old + i];
      // }

      // memcpy(pcmf32.data() + n_samples_from_old, s_queued_pcmf32.data(), n_samples_new * sizeof(float));
      pcmf32.insert(pcmf32.end(), s_queued_pcmf32.begin(), s_queued_pcmf32.end());
      // printf("processing: %d, threshold: %d\n", (int)pcmf32.size(), n_samples_iter_threshold);

      // print_array(pcmf32);

      s_queued_pcmf32.clear();
    }

    {
      int ret = whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size());
      if (ret != 0) {
        fprintf(stderr, "Failed to process audio, returned %d\n", ret);
        continue;
      }
    }

    {
      transcribed_msg msg;

      const int n_segments = whisper_full_n_segments(ctx);
      for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        msg.text += text;
      }

      bool speech_has_end = false;

      /**
       * Simple VAD from the "stream" example in whisper.cpp
       * https://github.com/ggerganov/whisper.cpp/blob/231bebca7deaf32d268a8b207d15aa859e52dbbe/examples/stream/stream.cpp#L378
       */
      /* Need enough accumulated audio to do VAD. */
      if ((int)pcmf32.size() >= n_samples_vad_window) {
        std::vector<float> pcmf32_window(pcmf32.end() - n_samples_vad_window, pcmf32.end());
        speech_has_end = vad_simple(pcmf32_window, WHISPER_SAMPLE_RATE, vad_last_ms,
                                    vad_thold, freq_thold, false);
        if (speech_has_end)
          printf("speech end detected\n");
      }

      /**
       * Clear audio buffer when the size exceeds iteration threshold or
       * speech end is detected.
       */
      if (pcmf32.size() > n_samples_iter_threshold || speech_has_end) {
        const auto t_now = std::chrono::high_resolution_clock::now();
        const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_last_iter).count();
        printf("iter took: %lldms\n", t_diff);
        t_last_iter = t_now;

        msg.is_partial = false;
        /**
         * Keep the last few samples in the audio buffer, so the next
         * iteration has a smoother start.
         */
        std::vector<float> last(pcmf32.end() - n_samples_keep_iter, pcmf32.end());
        pcmf32 = std::move(last);
      } else {
        msg.is_partial = true;
      }

      // printf("transcribed: %s\n", msg.text.c_str());

      std::lock_guard<std::mutex> lock(s_mutex);
      s_transcribed_msgs.insert(s_transcribed_msgs.end(), std::move(msg));
    }
  }
}