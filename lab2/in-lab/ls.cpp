#include "ls.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <map>

Ls::Ls() : a_(false), l_(false), R_(false), h_(false) {}
Ls::Ls(bool a, bool l, bool R, bool h)
    : a_(a), l_(l), R_(R), h_(h) {}

// Do NOT change this method
void Ls::Print(const StringMatrix &strings)
{
  for (const auto &row : strings)
  {
    for (size_t i = 0; i < row.size(); ++i)
    {
      std::cout << row[i];
      if (i != row.size() - 1) // Skip space after the last element
        std::cout << " ";
    }
    std::cout << std::endl;
  }
}

std::string getFileType(const std::string &path)
{
  struct stat st;
  if (lstat(path.c_str(), &st) == 0)
  {
    if (S_ISDIR(st.st_mode))
      return "DIRECTORY";
    if (S_ISLNK(st.st_mode))
      return "SOFTLINK";
  }
  return "FILE";
}

Ls::StringMatrix Ls::Run(const std::string &path)
{
  StringMatrix result;
  std::vector<std::string> entries;
  std::map<ino_t, std::vector<std::string>> hardlinks;

  DIR *dir = opendir(path.c_str());
  if (dir == nullptr)
  {
    std::cerr << "Error opening directory " << path << ": " << strerror(errno) << std::endl;
    return result;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr)
  {
    std::string name = entry->d_name;
    if (a_ || name[0] != '.')
    {
      entries.push_back(name);
    }
  }

  if (closedir(dir) == -1)
  {
    std::cerr << "Error closing directory " << path << ": " << strerror(errno) << std::endl;
  }

  std::sort(entries.begin(), entries.end());

  for (const auto &name : entries)
  {
    std::string relative_path = path + "/" + name;
    struct stat st;

    if (lstat(relative_path.c_str(), &st) == 0)
    {
      if (h_ && st.st_nlink > 1)
      {
        hardlinks[st.st_ino].push_back(relative_path);
      }
      else if (!h_)
      {
        std::vector<std::string> row;
        row.push_back(relative_path);
        if (l_)
        {
          row.push_back(getFileType(relative_path));
        }
        result.push_back(row);
      }
    }

    if (R_ && name != "." && name != ".." && getFileType(relative_path) == "DIRECTORY")
    {
      StringMatrix subdir_result = Run(relative_path);
      result.insert(result.end(), subdir_result.begin(), subdir_result.end());
    }
  }

  if (h_)
  {
    for (auto &[inode, paths] : hardlinks)
    {
      std::sort(paths.begin(), paths.end());
      std::vector<std::string> row = paths;
      if (l_)
      {
        row.push_back(getFileType(paths[0]));
      }
      result.push_back(row);
    }
  }

  std::sort(result.begin(), result.end());

  Print(result);
  return result;
}