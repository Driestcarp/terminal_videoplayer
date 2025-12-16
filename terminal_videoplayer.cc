#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <array>
#include <numeric>
#include <iomanip>

#define WINDOW_SIZE 2500

void resize_terminal(int rows, int cols) {
    std::cout << "\033[8;" << rows << ";" << cols << "t";
    std::cout.flush();
}

class RollingAverage
{
  std::array<int, WINDOW_SIZE> window{};
  std::array<int, WINDOW_SIZE>::iterator possition{window.begin()};
  int intPos{};
  bool windowFull{};
public:
  RollingAverage(){}

  void newValue(int value)
  {
    *possition = value;
    ++possition;
    ++intPos;
    if(possition == window.end())
    {
      possition = window.begin();
      windowFull = true;
      int intPos = 0;
    }
  }

  void reset()
  {
    window.fill(0);
    windowFull = false;
    possition = window.begin();
    intPos = 0;
  }

  int getAverage()
  {
    if(windowFull)
      return std::accumulate(window.begin(), window.end(), 0) / WINDOW_SIZE;
    return std::accumulate(window.begin(), possition, 0) / intPos;
  }
};

static const char* CURSOR_HOME = "\033[H";

static const std::array<std::string, 256> stringColor = []{
  std::array<std::string, 256> arr{};
  for (int i = 0; i < 256; ++i) {
    arr[i] = std::to_string(i);
  }
  return arr;
}();

inline void appendDoublePixel(
  std::string& out,
  uint8_t r_top, uint8_t g_top, uint8_t b_top,
  uint8_t r_bot, uint8_t g_bot, uint8_t b_bot,
  int& prev_r_top, int& prev_g_top, int& prev_b_top,
  int& prev_r_bot, int& prev_g_bot, int& prev_b_bot
) {
  if (r_top != prev_r_top || g_top != prev_g_top || b_top != prev_b_top) {
    prev_r_top = r_top;
    prev_g_top = g_top;
    prev_b_top = b_top;
    out += "\033[38;2;" + stringColor[r_top] + ";" + stringColor[g_top] + ";" + stringColor[b_top] + "m";
  }
  if (r_bot != prev_r_bot || g_bot != prev_g_bot || b_bot != prev_b_bot) {
    prev_r_bot = r_bot;
    prev_g_bot = g_bot;
    prev_b_bot = b_bot;
    out += "\033[48;2;" + stringColor[r_bot] + ";" + stringColor[g_bot] + ";" + stringColor[b_bot] + "m";
  }
  out += "▀";
}

int main(int argc, char** argv) {
	if (argc < 2) {
    std::cout << "Usage: ./termvid <video.mp4> [-loop] [-fps XX] [-stats] [-size x y]\n";
    return 1;
  }

  std::string filename;
  bool loop = false;
  double fps = 0;
  bool stats = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-loop") loop = true;
    else if (arg == "-fps" && i + 1 < argc) fps = std::atof(argv[++i]);
    else if (arg == "-stats") stats = true;
    else if (arg == "-size" && i + 2 < argc) {
      int rows = std::atoi(argv[++i]);
      int cols = std::atoi(argv[++i]);
      if (rows >= 5 && cols >= 10)
        resize_terminal(rows, cols);
      else {
        std::cerr << "Invalid size parameters. (5 and 10 is minimun)\n";
        return 1;
      }
    }
    else filename = arg;
  }

  if (filename.empty()) {
    std::cerr << "No video file specified.\n";
    return 1;
  }

  cv::VideoCapture cap(filename);
  if (!cap.isOpened()) {
    std::cerr << "Failed to open video.\n";
    return 1;
  }

  struct winsize w;
  
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  int term_w = w.ws_col;
  int term_h = w.ws_row;
  if (stats)
    term_h = w.ws_row - 1;

  if (term_w < 10 || term_h < 5) {
    std::cerr << "Terminal too small!\n";
    return 1;
  }

  if (fps <= 0) fps = cap.get(cv::CAP_PROP_FPS);
  if (fps < 1) fps = 30;
  double delay_ms = 1000.0 / fps;

  cv::Mat frame, resized;
  std::string ansi_buffer;
  ansi_buffer.reserve(10'000'000);

  std::cout << std::string(term_h, '\n');


  std::chrono::time_point<std::chrono::high_resolution_clock> start{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterRead{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterFor{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterWrite{};
  
  RollingAverage ReadAverage{};
  RollingAverage ForAverage{};
  RollingAverage WriteAverage{};

  do {
    while (true) {
      start = std::chrono::high_resolution_clock::now();
      if (!cap.read(frame)) break;
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
      if(term_w != w.ws_col || term_h != w.ws_row)
      {
        term_w = w.ws_col;
        term_h = w.ws_row - 1;
        /*ReadAverage.reset();
        ForAverage.reset();
        WriteAverage.reset();*/
      }
      cv::resize(frame, resized, cv::Size(term_w, term_h * 2), 0, 0, cv::INTER_AREA);

      ansi_buffer.clear();
      ansi_buffer += CURSOR_HOME;

      afterRead = std::chrono::high_resolution_clock::now();

        int prev_r_top = -1, prev_g_top = -1, prev_b_top = -1;
        int prev_r_bot = -1, prev_g_bot = -1, prev_b_bot = -1;

      for (int y = 0; y < resized.rows; y += 2) {
        const cv::Vec3b* row_top = resized.ptr<cv::Vec3b>(y);
        const cv::Vec3b* row_bot = (y + 1 < resized.rows) ? resized.ptr<cv::Vec3b>(y + 1) : row_top;

        for (int x = 0; x < resized.cols; ++x) {
          const cv::Vec3b& top = row_top[x];
          const cv::Vec3b& bot = row_bot[x];
          appendDoublePixel(
            ansi_buffer,
            top[2], top[1], top[0],
            bot[2], bot[1], bot[0],
            prev_r_top, prev_g_top, prev_b_top,
            prev_r_bot, prev_g_bot, prev_b_bot
          );
        }
        //ansi_buffer.push_back('\n');
      }

      afterFor = std::chrono::high_resolution_clock::now();

      ansi_buffer += "\033[0m";
      (void)write(STDOUT_FILENO, ansi_buffer.data(), ansi_buffer.size());

      afterWrite = std::chrono::high_resolution_clock::now();
      

      ReadAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterRead.time_since_epoch() - start.time_since_epoch()).count());
      ForAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterFor.time_since_epoch() - afterRead.time_since_epoch()).count());
      WriteAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterWrite.time_since_epoch() - afterFor.time_since_epoch()).count());
      
      int r = ReadAverage.getAverage();
      int f = ForAverage.getAverage();
      int w = WriteAverage.getAverage();
      int total = r + f + w;
      double percent_convert = double(100) / double(total);

      if (stats){
        std::cout << "Total: " << std::setw(10) << total;
        std::cout << " | Read: " << std::setw(10) << r << " - " << std::setprecision(3) << std::setw(4) << r * percent_convert << '%'; 
        std::cout << " | For: " << std::setw(10) << f << " - " << std::setprecision(3) << std::setw(4) << f * percent_convert << '%'; 
        std::cout << " | Write: " << std::setw(10) << w << " - " << std::setprecision(3) << std::setw(4) << w * percent_convert << '%' << std::flush; 
      }      
      std::this_thread::sleep_for(std::chrono::milliseconds((int)delay_ms));
    }

    if (loop) cap.set(cv::CAP_PROP_POS_FRAMES, 0);
  } while (loop);

  return 0;
}