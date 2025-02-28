#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Character {
    GLuint textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    GLuint advance;
};

std::map<char, Character> characters;
GLuint shaderProgram;
std::string inputText;
std::vector<std::string> commandOutput;
GLfloat cursorX = 50.0f;
GLfloat cursorY = 50.0f; // Ввод внизу
GLfloat scale = 1.0f;
bool showCursor = true;
double lastCursorBlinkTime = 0.0;
std::string username;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
})";

const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
})";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

void createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void loadCharacters() {
    FT_Library ft;
    FT_Init_FreeType(&ft);
    FT_Face face;
    FT_New_Face(ft, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face);
    FT_Set_Pixel_Sizes(face, 0, 24);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (GLubyte c = 0; c < 128; c++) {
        FT_Load_Char(face, c, FT_LOAD_RENDER);
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        characters[c] = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<GLuint>(face->glyph->advance.x)
        };
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void renderText(const std::string& text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color) {
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    for (auto c = text.begin(); c != text.end(); c++) {
        Character ch = characters[*c];
        GLfloat xpos = x + ch.bearing.x * scale;
        GLfloat ypos = y - (ch.size.y - ch.bearing.y) * scale;
        GLfloat w = ch.size.x * scale;
        GLfloat h = ch.size.y * scale;

        GLfloat vertices[6][4] = {
            {xpos,     ypos + h,   0.0, 0.0},
            {xpos,     ypos,       0.0, 1.0},
            {xpos + w, ypos,       1.0, 1.0},
            {xpos,     ypos + h,   0.0, 0.0},
            {xpos + w, ypos,       1.0, 1.0},
            {xpos + w, ypos + h,   1.0, 0.0}
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        x += (ch.advance >> 6) * scale;
    }
}

void executeCommand(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        commandOutput.push_back("Error: Failed to execute command.");
        return;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        commandOutput.push_back(buffer);
    }
    pclose(pipe);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_BACKSPACE && !inputText.empty()) {
            inputText.pop_back();
        }
        else if (key == GLFW_KEY_ENTER) {
            if (inputText == "exit" || inputText == "quit") {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                return;
            }
            executeCommand(inputText);
            inputText.clear();
        }
        else if (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL)) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        else if (key >= 32 && key <= 126) { // Печатные символы
            // Учитываем регистр символов
            if (mods & GLFW_MOD_SHIFT) {
                inputText += static_cast<char>(key); // Верхний регистр
            } else {
                inputText += static_cast<char>(std::tolower(key)); // Нижний регистр
            }
        }
    }
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "Xakterm 3D", NULL, NULL);
    glfwMakeContextCurrent(window);
    glewInit();
    createShaderProgram();
    loadCharacters();
    username = getlogin();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetKeyCallback(window, keyCallback);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        // Render command output
        GLfloat yPos = 500.0f;
        for (const auto& line : commandOutput) {
            renderText(line, 50.0f, yPos, scale, glm::vec3(1.0, 1.0, 1.0)); // Белый цвет для вывода
            yPos -= 24.0f;
        }

        // Render input line
        std::string prompt = username + "@xakterm:~$ " + inputText;
        renderText(prompt, 50.0f, 50.0f, scale, glm::vec3(0.0, 1.0, 0.0)); // Зелёный цвет для ввода

        // Render cursor
        double currentTime = glfwGetTime();
        if (currentTime - lastCursorBlinkTime > 0.5) {
            showCursor = !showCursor;
            lastCursorBlinkTime = currentTime;
        }
        if (showCursor) {
            renderText("_", 50.0f + prompt.length() * 14.0f, 50.0f, scale, glm::vec3(1.0, 1.0, 1.0));
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}