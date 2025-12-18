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
#include <algorithm>

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
  uint32_t top{};
  uint32_t bottom{};
  uint16_t x{};
  uint16_t y{};

  uint8_t get_top_r() const {return (0b00000000'00000000'00000000'11111111 & top);}
  uint8_t get_top_g() const {return (0b00000000'00000000'11111111'00000000 & top) >> 8;}
  uint8_t get_top_b() const {return (0b00000000'11111111'00000000'00000000 & top) >> 16;}
  uint8_t get_bottom_r() const {return (0b00000000'00000000'00000000'11111111 & bottom) ;}
  uint8_t get_bottom_g() const {return (0b00000000'00000000'11111111'00000000 & bottom) >> 8;}
  uint8_t get_bottom_b() const {return (0b00000000'11111111'00000000'00000000 & bottom) >> 16;}

  void set_top_r(uint8_t val) {top = (top & 0b11111111'11111111'11111111'00000000) | static_cast<uint32_t>(val);}
  void set_top_g(uint8_t val) {top = (top & 0b11111111'11111111'00000000'11111111) | (static_cast<uint32_t>(val) << 8);}
  void set_top_b(uint8_t val) {top = (top & 0b11111111'00000000'11111111'11111111) | (static_cast<uint32_t>(val) << 16);}
  void set_bottom_r(uint8_t val) {bottom = (bottom & 0b11111111'11111111'11111111'00000000) | static_cast<uint32_t>(val);}
  void set_bottom_g(uint8_t val) {bottom = (bottom & 0b11111111'11111111'00000000'11111111) | (static_cast<uint32_t>(val) << 8);}
  void set_bottom_b(uint8_t val) {bottom = (bottom & 0b11111111'00000000'11111111'11111111) | (static_cast<uint32_t>(val) << 16);}

  DoublePixel(){}
  DoublePixel(uint8_t top_r, uint8_t top_g, uint8_t top_b, uint8_t bottom_r, uint8_t bottom_g, uint8_t bottom_b, uint32_t x_pos, uint32_t y_pos)
  {
    top = static_cast<uint32_t>(top_r) | (static_cast<uint32_t>(top_g) << 8) | (static_cast<uint32_t>(top_b) << 16);
    bottom = static_cast<uint32_t>(bottom_r) | (static_cast<uint32_t>(bottom_g) << 8) | (static_cast<uint32_t>(bottom_b) << 16);
    x = x_pos;
    y = y_pos;
  }

  std::string getTopColorChar() const
  {
    return "\033[38;2;" + 
      stringColor[get_top_r()] + ";" + 
      stringColor[get_top_g()] + ";" + 
      stringColor[get_top_b()] + "m";
  }

  std::string getBottomColorChar() const
  {
    return "\033[48;2;" + 
      stringColor[get_bottom_r()] + ";" + 
      stringColor[get_bottom_g()] + ";" + 
      stringColor[get_bottom_b()] + "m";
  }

  std::string getPosChar() const
  {
    return "\033[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
  }

  bool operator==(DoublePixel const& other) const
  {
    return top == other.top && bottom == other.bottom;
  }

  bool operator!=(DoublePixel const& other) const
  {
    return !this->operator==(other);
  }

  bool operator<(DoublePixel other) const
  {
    if(top < other.top)
      return true;
    else if (top == other.top)
    {
      if(bottom < other.bottom)
        return true;
      else if (bottom == other.bottom)
      {
        if(x < other.x && y == other.y)
          return true;
        else
          return false;
      }
      else
        return false;
    }
    else
      return false;
  }
};

class Frame
{
  int x_size{};
  int y_size{};
  std::vector<DoublePixel> double_pixels{};

public:
  Frame(int width, int height)
  {
    x_size = width;
    y_size = height;
    double_pixels = std::vector<DoublePixel>{width * height, DoublePixel{}};
  }

  void AddDoublePixel(DoublePixel p)
  {
    double_pixels.push_back(p);
  }

  std::vector<DoublePixel> GetDiff(Frame new_frame)
  {
    if(x_size != new_frame.x_size || y_size != new_frame.y_size || double_pixels.size() != new_frame.double_pixels.size())
      return new_frame.double_pixels;
    
    
    std::vector<DoublePixel> diff{};
    std::vector<DoublePixel>::iterator other_it = new_frame.double_pixels.begin();
    for(std::vector<DoublePixel>::iterator it = double_pixels.begin(); it != double_pixels.end(); ++it)
    {
      if(*it != *other_it)
        diff.push_back(*other_it);
      ++other_it;
    }
    return diff;
  }
};

static const char* CURSOR_HOME = "\033[H";

std::string PixelVectorToString(std::vector<DoublePixel> v)
{
  if(v.size() == 0) return "";
  std::sort(v.begin(), v.end());
  std::vector<DoublePixel>::iterator pre_it = v.begin();
  std::vector<DoublePixel>::iterator cur_it = v.begin();
  std::string str{cur_it->getPosChar() + cur_it->getTopColorChar() + cur_it->getBottomColorChar()};
  ++cur_it;
  while (cur_it != v.end())
  {
    if(!(pre_it->x == cur_it->x - 1 && pre_it->y == cur_it->y))
      str += cur_it->getPosChar();
    if(pre_it->bottom != cur_it->bottom)
      str += cur_it->getBottomColorChar();
    if(pre_it->top != cur_it->top)
      str += cur_it->getTopColorChar();
    str += "▀";
    ++pre_it;
    ++cur_it;
  }
  return str;
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

  std::chrono::time_point<std::chrono::high_resolution_clock> start{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterRead{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterFor{};
  std::chrono::time_point<std::chrono::high_resolution_clock> afterWrite{};
  
  RollingAverage ReadAverage{};
  RollingAverage ForAverage{};
  RollingAverage WriteAverage{};
  RollingAverage FrameAverage{};
  RollingAverage BufAverage{};

  Frame pre_frame{term_w, term_h};
  Frame cur_frame{term_w, term_h};

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
      }

      pre_frame = cur_frame;
      cur_frame = Frame(term_w, term_h);
      cv::resize(frame, resized, cv::Size(term_w, term_h * 2), 0, 0, cv::INTER_AREA);

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
          cur_frame.AddDoublePixel(DoublePixel{top[2], top[1], top[0], bot[2], bot[1], bot[0], x, y/2});
        }
      }

      if(stats)
        afterFor = std::chrono::high_resolution_clock::now();
        
      std::string changed = PixelVectorToString(pre_frame.GetDiff(cur_frame));
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

        std::cout << "\033[38;2;255;255;255m\033[48;2;0;0;0m\033[" + std::to_string(term_h + 1) + ";0H";
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
