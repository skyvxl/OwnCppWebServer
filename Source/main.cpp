#include <nlohmann/json.hpp>
#include <shared_mutex>

#include "httplib.h"

struct Task {
  int id;
  std::string title;
  bool completed;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Task, id, title, completed)

int main() {
  std::vector<Task> todos{
      {1, "delectus aut autem", false},
      {2, "quis ut nam facilis et officia qui", false},
      {3, "fugiat veniam minus", false},
  };

  std::shared_mutex todos_mutex;

  httplib::Server srv;

  srv.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("Hello World", "text/plain");
  });

  srv.Get("/todos", [&todos, &todos_mutex](const httplib::Request&, httplib::Response& res) {
    try {
      // mutex lock
      std::shared_lock<std::shared_mutex> lock(todos_mutex);
      nlohmann::json jTodos = todos;
      res.status = 200;
      res.set_content(jTodos.dump(), "application/json");
      // mutex unlock
    } catch (const std::exception&) {
      res.status = 500;
      res.set_content("server error", "application/json");
    }
  });

  srv.Post("/todos", [&todos, &todos_mutex](const httplib::Request& req, httplib::Response& res) {
    try {
      auto body = nlohmann::json::parse(req.body);
      if (!body.contains("title")) {
        res.status = 400;
        res.set_content(R"({"error":"Missing title"})", "application/json");
        return;
      }
      // mutex lock
      std::unique_lock<std::shared_mutex> lock(todos_mutex);

      int next_id = todos.empty() ? 0 : todos.back().id + 1;
      Task newTask = {next_id, body["title"].get<std::string>(), body.value("completed", false)};
      todos.push_back(newTask);

      nlohmann::json jTodos = todos;

      res.status = 201;
      res.set_content(jTodos.dump(), "application/json");

      // mutex unlock
    } catch (const std::exception&) {
      res.status = 400;
      res.set_content(R"({"error":"Invalid JSON"})", "application/json");
    }
  });

  srv.listen("0.0.0.0", 8080);
  return 0;
}