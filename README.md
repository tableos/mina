### Setup

```bash
git clone https://github.com/tableos/mina.git
cd mina
./setup.sh
```

### Usage

* `npm run test` to run the real-time stt engine with a recorded speech clip.
  * It will keep running after the clip is transcribed, hit `Ctrl-C` to stop.
* `npm start` to start the electron app. It will listen to your default input device and transcribe anything it records.
* **On macOS**, If the audio is choppy when this app is running, try restarting the audio stack with

  ```bash
  sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod
  ```

### Development

* `npm run build` to compile C/C++ code in `<project_root_dir>/native/`.
* `npm run build-verbose` to see the actual command `node-gyp` is executing to compile C/C++ code. Useful when you want to check whether compiler flags are set correctly.
* `git submodule update --remote whisper.cpp` to update whisper.cpp
