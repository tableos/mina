#ifndef STT_WHISPER_H_
#define STT_WHISPER_H_

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct transcribed_msg {
  std::string text;
  bool is_partial;
};

class RealtimeSttWhisper
{
 public:
  RealtimeSttWhisper(const std::string& path_model);
  ~RealtimeSttWhisper();
  void AddAudioData(const std::vector<float>& new_data);
  std::vector<transcribed_msg> GetTranscribed();

 private:
  struct whisper_context* ctx;
  std::atomic<bool> is_running;
  std::vector<float> s_queued_pcmf32;
  std::vector<transcribed_msg> s_transcribed_msgs;
  std::mutex s_mutex;  // for accessing shared variables from both main thread and worker thread
  std::thread worker;
  void Run();
};

#endif  // STT_WHISPER_H_
