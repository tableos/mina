echo "Init whisper.cpp git submodule..."
git submodule init
echo "Installing dependencies and compiling C/C++ native addons..."
npm install
echo "Downloading whisper model..."
bash whisper.cpp/models/download-ggml-model.sh tiny.en
echo "Creating audio file for testing..."
ffmpeg -i whisper.cpp/samples/jfk.wav -ar 16000 -ac 1 -c:a pcm_f32le \
-f f32le whisper.cpp/samples/jfk.raw
