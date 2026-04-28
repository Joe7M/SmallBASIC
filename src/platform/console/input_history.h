// This file is part of SmallBASIC
//
// Copyright(C) 2001-2026 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef INPUTHISTORY_H
#define INPUTHISTORY_H

#include <string>
#include <vector>
#include <algorithm>

class InputHistory {
  public:

  explicit InputHistory() = default;
  ~InputHistory() = default;

  // not implemented by default
  InputHistory(const InputHistory &) = delete;
  InputHistory &operator=(const InputHistory &) = delete;

  /**
   * @brief Adds a new command string to the history stack.
   * Resets navigation index upon adding a new item.
   */
  void push(const char *input) {
    std::string new_entry(input);
    history_stack.push_back(new_entry);
    current_index = ((int)history_stack.size());
  }

  /**
   * @brief Navigates up the history stack.
   * @param dest Buffer to store the selected entry.
   * @param size Maximum size of the destination buffer.
   * @return true if an item was successfully retrieved.
   */
  bool up(char *dest, int size) {
    bool result = false;
    if (!history_stack.empty() && current_index > 0) {
      // navigate to earlier entries
      result = copy(current_index - 1, dest, size);
    }
    return result;
  }

  /**
   * @brief Navigates down the history stack.
   * @param dest Buffer to store the selected entry.
   * @param size Maximum size of the destination buffer.
   * @return true if an item was successfully retrieved.
   */
  bool down(char *dest, int size) {
    bool result = false;
    if (!history_stack.empty() && current_index < (int)history_stack.size()) {
      // navigate to later entries
      result = copy(current_index + 1, dest, size);
    }
    return result;
  }

  private:

  /**
   * copy the current history text to the destination
   */
  bool copy(int index, char *dest, int size) {
    bool result = false;
    if (index >= 0 && index < (int)history_stack.size()) {
      int destLen = dest != nullptr ? strlen(dest) : 0;
      const std::string& entry = history_stack[index];
      strlcpy(dest, entry.c_str(), size);
      result = true;
      current_index = index;

      // echo the result
#if USE_TERM_IO
      if (destLen > 0) {
        // go back n positions before clearing the line
        printf("\033[%dD\033[K%s", destLen, dest);
      } else {
        printf("\033[K%s", dest);
      }
      fflush(stdout);
#endif
    }
    return result;
  }

  std::vector<std::string> history_stack {};
  int current_index {};
};

#endif // INPUTHISTORY_H
