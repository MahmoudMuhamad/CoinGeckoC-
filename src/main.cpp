#include "app_state.h"
#include "coingecko_client.h"
#include "storage.h"
#include "utils.h"

#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

static bool is_favorite(const AppState& st, const std::string& id) {
    return std::find(st.favorites.begin(), st.favorites.end(), id) != st.favorites.end();
}

static void toggle_favorite(AppState& st, const std::string& id) {
    auto it = std::find(st.favorites.begin(), st.favorites.end(), id);
    if (it == st.favorites.end()) st.favorites.push_back(id);
    else st.favorites.erase(it);
}

// Background refresh loop
static void refresh_loop(AppState* st, std::atomic<bool>* running, CoinGeckoClient client) {
    while (running->load()) {
        bool do_refresh = false;
        int sleep_s = 1;

        {
            std::lock_guard<std::mutex> lk(st->data_mutex);
            do_refresh = st->auto_refresh && !st->cache_by_coin.empty();
            sleep_s = std::max(1, st->refresh_seconds);
        }

        if (do_refresh) {
            std::vector<std::string> ids;
            {
                std::lock_guard<std::mutex> lk(st->data_mutex);
                ids.reserve(st->cache_by_coin.size());
                for (const auto& [id, _] : st->cache_by_coin) ids.push_back(id);
            }

            for (const auto& id : ids) {
                std::string err;
                auto q = client.fetch_simple_price(id, err);
                std::lock_guard<std::mutex> lk(st->data_mutex);
                if (q) st->cache_by_coin[id] = *q;
                if (!err.empty()) st->last_error = err;
            }
        }

        for (int i = 0; i < sleep_s * 10 && running->load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

int main() {
    AppState st;

    // load persisted settings & favorites
    {
        std::string err;
        Storage::load_favorites(st, err);
        Storage::load_settings(st, err);
    }

    CoinGeckoClient client;

    // Init window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1100, 700, "AirWatch (CoinGecko)", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL loader!\n";
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::atomic<bool> running{true};
    std::thread bg(refresh_loop, &st, &running, client);

    char coin_input[64] = "bitcoin";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("AirWatch — CoinGecko");

        ImGui::Text("Enter coin id (examples: bitcoin, ethereum, solana, dogecoin)");
        ImGui::InputText("Coin ID", coin_input, sizeof(coin_input));
        ImGui::SameLine();

        if (ImGui::Button("Add/Watch")) {
            std::string id = to_lower(trim(std::string(coin_input)));
            std::string err;
            auto q = client.fetch_simple_price(id, err);
            std::lock_guard<std::mutex> lk(st.data_mutex);
            if (q) st.cache_by_coin[id] = *q;
            st.last_error = err;
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            std::string id = to_lower(trim(std::string(coin_input)));
            std::lock_guard<std::mutex> lk(st.data_mutex);
            st.cache_by_coin.erase(id);
        }

        if (ImGui::Button("Refresh Now")) {
            std::vector<std::string> ids;
            {
                std::lock_guard<std::mutex> lk(st.data_mutex);
                for (const auto& [id, _] : st.cache_by_coin) ids.push_back(id);
            }
            for (const auto& id : ids) {
                std::string err;
                auto q = client.fetch_simple_price(id, err);
                std::lock_guard<std::mutex> lk(st.data_mutex);
                if (q) st.cache_by_coin[id] = *q;
                if (!err.empty()) st.last_error = err;
            }
        }

        ImGui::Separator();
        ImGui::Checkbox("Auto Refresh (background thread)", &st.auto_refresh);
        ImGui::SameLine();
        ImGui::InputInt("Refresh seconds", &st.refresh_seconds);
        if (st.refresh_seconds < 1) st.refresh_seconds = 1;

        ImGui::Separator();
        if (ImGui::Button("Save Favorites/Settings")) {
            std::string err;
            Storage::save_favorites(st, err);
            Storage::save_settings(st, err);
            std::lock_guard<std::mutex> lk(st.data_mutex);
            if (!err.empty()) st.last_error = err;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Favorites/Settings")) {
            std::string err;
            Storage::load_favorites(st, err);
            Storage::load_settings(st, err);
            std::lock_guard<std::mutex> lk(st.data_mutex);
            if (!err.empty()) st.last_error = err;
        }

        {
            std::lock_guard<std::mutex> lk(st.data_mutex);
            if (!st.last_error.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "Last error: %s", st.last_error.c_str());
            }
        }

        ImGui::Separator();
        ImGui::Text("Watched Coins");

        // Snapshot for UI (avoid holding mutex during ImGui table)
        std::unordered_map<std::string, CoinQuote> copy;
        std::vector<std::string> favs;
        {
            std::lock_guard<std::mutex> lk(st.data_mutex);
            copy = st.cache_by_coin;
            favs = st.favorites;
        }

        if (ImGui::BeginTable("coins_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Fav");
            ImGui::TableSetupColumn("Coin");
            ImGui::TableSetupColumn("USD");
            ImGui::TableSetupColumn("24h %");
            ImGui::TableSetupColumn("Notes");
            ImGui::TableHeadersRow();

            for (const auto& [id, q] : copy) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                bool fav = std::find(favs.begin(), favs.end(), id) != favs.end();
                if (ImGui::SmallButton((std::string(fav ? "★##" : "☆##") + id).c_str())) {
                    std::lock_guard<std::mutex> lk(st.data_mutex);
                    toggle_favorite(st, id);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(id.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", q.usd);

                ImGui::TableNextColumn();
                ImGui::Text("%.2f", q.usd_24h_change);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("CoinGecko /simple/price");
            }

            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // shutdown
    running.store(false);
    if (bg.joinable()) bg.join();

    {
        std::string err;
        Storage::save_favorites(st, err);
        Storage::save_settings(st, err);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
