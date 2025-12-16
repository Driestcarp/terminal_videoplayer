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
#include <sstream>
#include <queue>

#define WINDOW_SIZE 5000

static const std::array<std::string, 256> stringColor = []{
  std::array<std::string, 256> arr{};
  for (int i = 0; i < 256; ++i) {
    arr[i] = std::to_string(i);
  }
  return arr;
}();


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
    if(intPos > 0)
      return std::accumulate(window.begin(), possition, 0) / intPos;
    return 1;
  }
};

struct DoublePixel
{
  uint8_t top_r{};
  uint8_t top_g{};
  uint8_t top_b{};
  uint8_t bottom_r{};
  uint8_t bottom_g{};
  uint8_t bottom_b{};

  DoublePixel(){}
  DoublePixel(uint8_t top_red, uint8_t top_green, uint8_t top_blue, uint8_t bottom_red, uint8_t bottom_green, uint8_t bottom_blue)
  {
    top_r = top_red;
    top_g = top_green;
    top_b = top_blue;
    bottom_r = bottom_red;
    bottom_g = bottom_green;
    bottom_b = bottom_blue;
  }

  std::string getColorChar() const
  {
    return 
      "\033[38;2;" + 
        stringColor[top_r] + ";" +
        stringColor[top_g] + ";" +
        stringColor[top_b] + "m" +
      "\033[48;2;" +
        stringColor[bottom_r] + ";" +
        stringColor[bottom_g] + ";" +
        stringColor[bottom_b] + "m" +
      "▀";
  }

  bool operator==(DoublePixel const& other) const
  {
    return top_r == other.top_r &&
      top_g == other.top_g &&
      top_b == other.top_b &&
      bottom_r == other.bottom_r &&
      bottom_g == other.bottom_g &&
      bottom_b == other.bottom_b;
  }

  bool operator!=(DoublePixel const& other) const
  {
    return !this->operator==(other);
  }
};

class Screen
{
public:
  Screen(int width, int height)
  {
    x_size = width;
    y_size = height;
    changes.reserve(10'000'000);
    double_pixels = std::vector<DoublePixel>{width * height, DoublePixel{}};
    
  }

  void newSize(int width, int height)
  {
    x_size = width;
    y_size = height;
    double_pixels = std::vector<DoublePixel>{width * height, DoublePixel{}};
  }

  void newFrame()
  {
    changes.clear();
    pixel_it = double_pixels.begin();
    it_pos = 0;
    last_change = -100;
    x_pos = 0;
    y_pos = 0;
  }

  void nextDoublePixel(DoublePixel const& pixel)
  {
    if(*pixel_it != pixel)
    {
      double_pixels[it_pos] = pixel;
      if(last_change != it_pos - 1)
        changes.append("\033[" + std::to_string(y_pos + 1) + ";" + std::to_string(x_pos + 1) + "H" + pixel.getColorChar());
      else if(last_new == pixel)
        changes.append("▀");
      else
        changes.append(pixel.getColorChar());
      last_change = it_pos;
      last_new = pixel;
    }

    ++it_pos;
    ++pixel_it;
    ++x_pos;
    if(x_pos >= x_size)
    {
      x_pos = 0;
      ++y_pos;
    }
  }

  std::string change() const
  {
    return changes + "\033[0m" + "\033[" + std::to_string(y_size + 1) + ";0H";
  }

private:
  int x_pos;
  int y_pos;
  int x_size;
  int y_size;
  int it_pos;
  int last_change;
  DoublePixel last_new;
  std::string changes;
  std::vector<DoublePixel>::iterator pixel_it;
  std::vector<DoublePixel> double_pixels;
};

static const char* CURSOR_HOME = "\033[H";



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
    std::cout << "Usage: ./termvid <video.mp4> [-loop] [-fps <XX | -1 for uncapped>] [-stats]\n";
    return 1;
  }

  std::string filename;
  bool loop = false;
  bool stats = false;
  bool unlimitedFps = false;
  double fps = 0;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-loop") loop = true;
    else if (arg == "-stats") stats = true;
    else if (arg == "-fps" && i + 1 < argc) fps = std::atof(argv[++i]);
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
  int term_h = w.ws_row - (stats ? 1 : 0);

  if (term_w < 10 || term_h < 5) {
    std::cerr << "Terminal too small!\n";
    return 1;
  }

  double delay_ms{};
  if(fps < 0)
    unlimitedFps = true;
  else
  {
    if (fps == 0) fps = cap.get(cv::CAP_PROP_FPS);
    if (fps < 1) fps = 30;
    delay_ms = 1000.0 / fps;
  }

  cv::Mat frame, resized;
  std::string ansi_buffer;
  ansi_buffer.reserve(10'000'000);

  std::cout << std::string(term_h, '\n');

  Screen screen{term_w, term_h};

  std::chrono::time_point<std::chrono::high_resolution_clock> start{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterRead{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterFor{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterWrite{};
  
  RollingAverage ReadAverage{};
  RollingAverage ForAverage{};
  RollingAverage WriteAverage{};
  RollingAverage FrameAverage{};
  RollingAverage BufAverage{};

  do {
    while (true) {
      start = std::chrono::high_resolution_clock::now();
      std::string been_resized{};
      if (!cap.read(frame)) break;
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
      if(term_w != w.ws_col || term_h != w.ws_row - (stats ? 1 : 0))
      {
        been_resized = "Has been resized";
        term_w = w.ws_col;
        term_h = w.ws_row - (stats ? 1 : 0);
        screen.newSize(term_w, term_h);
        /*ReadAverage.reset();
        ForAverage.reset();
        WriteAverage.reset();*/
      }
      screen.newFrame();
      cv::resize(frame, resized, cv::Size(term_w, term_h * 2), 0, 0, cv::INTER_AREA);

      ansi_buffer.clear();
      ansi_buffer += CURSOR_HOME;

      if(stats)
        afterRead = std::chrono::high_resolution_clock::now();

      for (int y = 0; y < resized.rows; y += 2) {
        const cv::Vec3b* row_top = resized.ptr<cv::Vec3b>(y);
        const cv::Vec3b* row_bot = (y + 1 < resized.rows) ? resized.ptr<cv::Vec3b>(y + 1) : row_top;

        int prev_r_top = -1, prev_g_top = -1, prev_b_top = -1;
        int prev_r_bot = -1, prev_g_bot = -1, prev_b_bot = -1;

        for (int x = 0; x < resized.cols; ++x) {
          const cv::Vec3b& top = row_top[x];
          const cv::Vec3b& bot = row_bot[x];
          screen.nextDoublePixel(DoublePixel{top[2], top[1], top[0], bot[2], bot[1], bot[0]});
          /*appendDoublePixel(
            ansi_buffer,
            top[2], top[1], top[0],
            bot[2], bot[1], bot[0],
            prev_r_top, prev_g_top, prev_b_top,
            prev_r_bot, prev_g_bot, prev_b_bot
          );*/
        }
      }

      if(stats)
        afterFor = std::chrono::high_resolution_clock::now();

      //ansi_buffer += "\033[0m";
      //(void)write(STDOUT_FILENO, ansi_buffer.data(), ansi_buffer.size());
      std::string changed = screen.change();
      (void)write(STDOUT_FILENO, changed.data(), changed.size());

      if(stats)
        afterWrite = std::chrono::high_resolution_clock::now();
      
      if(stats)
      {
        ReadAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterRead.time_since_epoch() - start.time_since_epoch()).count());
        ForAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterFor.time_since_epoch() - afterRead.time_since_epoch()).count());
        WriteAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(afterWrite.time_since_epoch() - afterFor.time_since_epoch()).count());
        BufAverage.newValue(changed.size());

        int r = ReadAverage.getAverage();
        int f = ForAverage.getAverage();
        int w = WriteAverage.getAverage();
        int total = r + f + w;
        double percent_convert = double(100) / double(total);

        std::cout << "Total: " << std::setw(6) << total;
        std::cout << " | Read: " << std::setw(6) << r << " - " << std::setprecision(2) << std::setw(4) << r * percent_convert << '%'; 
        std::cout << " | For: " << std::setw(6) << f << " - " << std::setprecision(2) << std::setw(4) << f * percent_convert << '%'; 
        std::cout << " | Write: " << std::setw(6) << w << " - " << std::setprecision(2) << std::setw(4) << w * percent_convert << '%';
        std::cout << " | FPS: " << std::setprecision(2) << std::setw(4) << 1000000 / FrameAverage.getAverage();
        std::cout << " | Buf size: " << std::setw(8) << BufAverage.getAverage(); 
        std::cout << " | " << term_w << "x" << term_h << " = " << term_w*term_h << std::flush;
      }

      if(!unlimitedFps)
        std::this_thread::sleep_for(std::chrono::microseconds((int)(delay_ms * 1000)) - std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch() - start.time_since_epoch()));
      FrameAverage.newValue(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch() - start.time_since_epoch()).count());
    }

    if (loop) cap.set(cv::CAP_PROP_POS_FRAMES, 0);
  } while (loop);

  return 0;
}
