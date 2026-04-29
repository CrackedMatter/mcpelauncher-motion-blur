#define LOG_TAG "MotionBlur"
#include "motionblur.hpp"
#include "gamewindow.hpp"
#include "menu.hpp"
#include <GLES2/gl2.h>
#include <android/log_macros.h>
#include <cmath>
#include <glaze/json.hpp>

struct Config {
    float intensity     = 0.5f;
    bool  enableInMenus = false;
};

static Config config;

static glz::sv configPath = "/data/data/com.mojang.minecraftpe/motion_blur_config.json";

static constexpr const char* vertexShaderSource = /* language=glsl */ R"(
attribute vec4 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    gl_Position = aPosition;
    vTexCoord = aTexCoord;
}
)";

static constexpr const char* fragmentShaderSource = /* language=glsl */ R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uCurrentFrame;
uniform sampler2D uPreviousFrame;
uniform float uBlendFactor;
void main() {
    vec4 currentFrameColor = texture2D(uCurrentFrame, vTexCoord);
    vec4 previousFrameColor = texture2D(uPreviousFrame, vTexCoord);
    gl_FragColor = mix(currentFrameColor, previousFrameColor, uBlendFactor * (1.0 - 0.5 * exp2(-10.0 * distance(currentFrameColor, previousFrameColor))));
}
)";

static GLuint currentFrameTexture   = 0;
static GLuint previousFrameTexture  = 0;
static GLuint shaderProgram         = 0;
static GLuint vertexBuffer          = 0;
static GLuint indexBuffer           = 0;
static GLint  positionLocation      = -1;
static GLint  texCoordLocation      = -1;
static GLint  currentFrameLocation  = -1;
static GLint  previousFrameLocation = -1;
static GLint  blendFactorLocation   = -1;

static void loadConfig() {
    std::string buffer;
    if (auto ec = glz::read_file_json(config, configPath, buffer))
        ALOGE("Failed to load motion blur config: %s", glz::format_error(ec, buffer).c_str());
}

static void saveConfig() {
    std::string buffer;
    if (auto ec = glz::write_file_json<glz::opts{.prettify = true}>(config, configPath, buffer))
        ALOGE("Failed to save motion blur config: %s", glz::format_error(ec, buffer).c_str());
}

void initMotionBlur() {
    loadConfig();
    saveConfig();

    MenuEntryABI menuEntry{
        .name  = "Motion Blur Settings",
        .click = [](void*) {
            ControlABI controls[3];

            controls[0].type             = 2;
            controls[0].data.sliderfloat = {
                .label    = "Intensity",
                .min      = 0.0f,
                .def      = config.intensity,
                .max      = 1.0f,
                .user     = nullptr,
                .onChange = [](void*, float value) { config.intensity = value; },
            };

            controls[1].type           = 1;
            controls[1].data.sliderint = {
                .label    = "Enable in menus - 0: off (recommended), 1: on",
                .min      = 0,
                .def      = config.enableInMenus,
                .max      = 1,
                .user     = nullptr,
                .onChange = [](void*, int value) { config.enableInMenus = value; },
            };

            controls[2].type        = 0;
            controls[2].data.button = {
                .label   = "Save",
                .user    = nullptr,
                .onClick = [](void*) { saveConfig(); },
            };

            showWindow("Motion Blur Settings", false, nullptr, [](void*) { saveConfig(); }, std::size(controls), controls);
        },
    };

    addMenu(1, &menuEntry);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    positionLocation      = glGetAttribLocation(shaderProgram, "aPosition");
    texCoordLocation      = glGetAttribLocation(shaderProgram, "aTexCoord");
    currentFrameLocation  = glGetUniformLocation(shaderProgram, "uCurrentFrame");
    previousFrameLocation = glGetUniformLocation(shaderProgram, "uPreviousFrame");
    blendFactorLocation   = glGetUniformLocation(shaderProgram, "uBlendFactor");

    glGenTextures(1, &currentFrameTexture);
    glBindTexture(GL_TEXTURE_2D, currentFrameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &previousFrameTexture);
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // clang-format off
    GLfloat vertices[] = {
        // positions  // texture coords
        -1.0f, 1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f,  -1.0f, 1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f,
    };

    GLushort indices[] = {
        0, 1, 2,
        0, 2, 3,
    };
    // clang-format on

    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

void onSwapBuffers(void*, EGLDisplay display, EGLSurface surface) {
    if (!config.enableInMenus && !isMouseLocked(getPrimaryWindow()))
        return;

    EGLint width, height;
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);

    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);

    // copy current framebuffer
    glBindTexture(GL_TEXTURE_2D, currentFrameTexture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);

    // texture slots 10 and 11 are definitely not used by the game
    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, currentFrameTexture);
    glUniform1i(currentFrameLocation, 10);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glUniform1i(previousFrameLocation, 11);

    // ease out for a more gradually increasing blend factor (todo: should also account for frame delta time)
    glUniform1f(blendFactorLocation, 1.0f - std::exp2(-5.0f * config.intensity));

    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

    glEnableVertexAttribArray(static_cast<GLuint>(positionLocation));
    glVertexAttribPointer(static_cast<GLuint>(positionLocation), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);

    glEnableVertexAttribArray(static_cast<GLuint>(texCoordLocation));
    glVertexAttribPointer(static_cast<GLuint>(texCoordLocation), 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<void*>(2 * sizeof(GLfloat)));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

    // use blended result for next frame
    glBindTexture(GL_TEXTURE_2D, previousFrameTexture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);
}
