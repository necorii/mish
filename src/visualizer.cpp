#include "visualizer.h"
#include <cmath>
#include <cstdint>
#include <complex>
#include <vector>
#include <algorithm>

typedef std::complex<float> Complex;
const float PI = 3.14159265358979323846f;

// Cooley-Tukey FFT
void runFFT(std::vector<Complex>& a) {
    size_t n = a.size();
    if (n <= 1) return;

    std::vector<Complex> a0(n / 2), a1(n / 2);
    for (size_t i = 0; 2 * i < n; i++) {
        a0[i] = a[2 * i];
        a1[i] = a[2 * i + 1];
    }

    runFFT(a0);
    runFFT(a1);

    float angle = 2 * PI / n;
    Complex w(1), wn(std::cos(angle), -std::sin(angle));
    for (size_t i = 0; 2 * i < n; i++) {
        Complex t = w * a1[i];
        a[i] = a0[i] + t;
        a[i + n / 2] = a0[i] - t;
        w *= wn;
    }
}

Visualizer::Visualizer(unsigned int width, unsigned int height) 
    : m_width(width), 
      m_height(height), 
      m_primaryBuffer({width, height}), 
      m_feedbackBuffer({width, height}),
      m_feedbackSprite(m_feedbackBuffer.getTexture()), // Now safely binds the texture!
      m_rotationAngle(0.0f) {
    
    m_magnitudes.resize(512, 0.0f); // Map to 512 frequency bars

    m_primaryBuffer.clear(sf::Color::Black);
    m_feedbackBuffer.clear(sf::Color::Black);

    // Dynamic central ring driven by audio peaks
    m_innerRing = sf::VertexArray(sf::PrimitiveType::TriangleFan, 362); 
}

void Visualizer::update(const sf::Sound& sound, const sf::SoundBuffer& buffer) {
    // High-velocity clock to drive fast-paced chaos and smooth orbits simultaneously
    float time = m_colorClock.getElapsedTime().asSeconds();
    float fastTime = time * 8.0f; 
    
    float bassIntensity = 0.0f;
    float midIntensity = 0.0f;
    float trebleIntensity = 0.0f;

    if (sound.getStatus() == sf::Sound::Status::Playing) {
        sf::Time offset = sound.getPlayingOffset();
        unsigned int sampleRate = buffer.getSampleRate();
        unsigned int channelCount = buffer.getChannelCount();
        const std::int16_t* samples = buffer.getSamples();
        
        std::uint64_t currentSampleIdx = static_cast<std::uint64_t>(offset.asSeconds() * sampleRate * channelCount);
        std::uint64_t totalSamples = buffer.getSampleCount();

        const size_t fftSize = 2048;
        std::vector<Complex> fftBuffer(fftSize, 0.0f);

        for (size_t i = 0; i < fftSize; ++i) {
            std::uint64_t idx = currentSampleIdx + (i * channelCount);
            if (idx < totalSamples) {
                float monoSample = 0.0f;
                for (unsigned int c = 0; c < channelCount; ++c) {
                    monoSample += samples[idx + c];
                }
                monoSample /= (channelCount * 32768.0f);

                float windowFactor = 0.5f * (1.0f - std::cos(2.0f * PI * i / (fftSize - 1)));
                fftBuffer[i] = monoSample * windowFactor;
            } else {
                fftBuffer[i] = 0.0f;
            }
        }

        runFFT(fftBuffer);

        float minFreq = 20.0f;
        float maxFreq = 16000.0f;
        float binWidth = static_cast<float>(sampleRate) / fftSize;

        for (unsigned int i = 0; i < 512; i++) {
            float normX = static_cast<float>(i) / 512.0f;
            float targetFreq = minFreq * std::pow(maxFreq / minFreq, normX);
            float binIndex = targetFreq / binWidth;

            size_t lowIdx = std::clamp(static_cast<size_t>(std::floor(binIndex)), size_t(0), fftSize / 2 - 2);
            size_t highIdx = lowIdx + 1;
            float interpolation = binIndex - lowIdx;

            float magLow = std::abs(fftBuffer[lowIdx]);
            float magHigh = std::abs(fftBuffer[highIdx]);
            float rawMagnitude = magLow + (magHigh - magLow) * interpolation;

            float normalizedMag = rawMagnitude / (fftSize / 2.0f);
            float processedMag = std::log10(1.0f + normalizedMag * 60.0f); 

            if (processedMag > m_magnitudes[i]) {
                m_magnitudes[i] = processedMag;
            } else {
                m_magnitudes[i] += (processedMag - m_magnitudes[i]) * 0.12f; 
            }

            if (i < 35)   bassIntensity += m_magnitudes[i];
            if (i >= 35 && i < 220) midIntensity += m_magnitudes[i];
            if (i >= 220) trebleIntensity += m_magnitudes[i];
        }
        bassIntensity /= 35.0f;
        midIntensity /= 185.0f;
        trebleIntensity /= 292.0f;
    } else {
        for (unsigned int i = 0; i < 512; i++) {
            m_magnitudes[i] += (0.0f - m_magnitudes[i]) * 0.08f;
        }
    }

    // --- PHASE 1: THE FEEDBACK CYCLONE WARP ---
    m_feedbackSprite.setTexture(m_feedbackBuffer.getTexture());
    m_feedbackSprite.setOrigin({m_width / 2.0f, m_height / 2.0f});

    // Subtly drift the screen center while injecting high-speed bass jitter (Combining styles!)
    float driftX = std::sin(time * 1.0f) * 12.0f;
    float driftY = std::cos(time * 1.3f) * 12.0f;
    float jitterX = ((float)rand() / RAND_MAX - 0.5f) * 14.0f * bassIntensity;
    float jitterY = ((float)rand() / RAND_MAX - 0.5f) * 14.0f * bassIntensity;
    
    m_feedbackSprite.setPosition({m_width / 2.0f + driftX + jitterX, m_height / 2.0f + driftY + jitterY});

    // Intense feedback zooming + dual-direction spin control
    float zoomFactor = 0.975f + (bassIntensity * 0.06f);
    float spinDirection = (std::sin(time * 0.4f) > 0.0f) ? 1.0f : -1.0f;
    float rotationDelta = (0.8f + (trebleIntensity * 8.0f)) * spinDirection;
    m_rotationAngle += rotationDelta;

    m_feedbackSprite.setScale({zoomFactor, zoomFactor});
    m_feedbackSprite.setRotation(sf::degrees(m_rotationAngle * 0.12f));
    
    // Ghostly trails that shift color over time
    std::uint8_t trailR = static_cast<std::uint8_t>(245 + std::sin(time) * 8.0f);
    std::uint8_t trailG = static_cast<std::uint8_t>(245 + std::sin(time + 2.094f) * 8.0f);
    std::uint8_t trailB = static_cast<std::uint8_t>(245 + std::sin(time + 4.188f) * 8.0f);
    m_feedbackSprite.setColor(sf::Color(trailR, trailG, trailB, 246)); 

    m_primaryBuffer.clear(sf::Color(0, 0, 0, 255));
    m_primaryBuffer.draw(m_feedbackSprite);

    sf::Vector2f center(m_width / 2.0f, m_height / 2.0f);

    // --- PHASE 2: CHAOTIC GEOMETRIC FRACTURE WEB & SPARKS ---
    // Draw the unstable, sharp laser web spanning the dynamic outer bounds
    sf::VertexArray web(sf::PrimitiveType::LineStrip, 90);
    for (size_t i = 0; i < 90; ++i) {
        float angle = (i * 4.0f * PI / 45.0f) + (time * 0.8f);
        int magIdx = static_cast<int>(i * (512.0f / 90.0f)) % 512;
        
        float dist = 100.0f + (m_magnitudes[magIdx] * 290.0f) + (std::sin(angle * 6.0f + fastTime) * 35.0f);
        float x = center.x + std::cos(angle) * dist;
        float y = center.y + std::sin(angle) * dist;

        std::uint8_t r = static_cast<std::uint8_t>(std::sin(angle * 2.0f + time) * 127.0f + 128.0f);
        std::uint8_t g = static_cast<std::uint8_t>(std::cos(angle * 3.0f + time * 1.2f) * 127.0f + 128.0f);
        std::uint8_t b = static_cast<std::uint8_t>(std::sin(angle * 1.5f - time * 1.5f) * 127.0f + 128.0f);

        web[i] = sf::Vector2f(x,y), sf::Color(r,g,b,200), sf::Vector2f()
    }
    m_primaryBuffer.draw(web);

    // Random explosive neon static sparks shooting out of the center
    if (bassIntensity > 0.45f || trebleIntensity > 0.35f) {
        sf::VertexArray sparks(sf::PrimitiveType::Lines, 16);
        for (size_t i = 0; i < 16; i += 2) {
            float angle = ((float)rand() / RAND_MAX) * 2.0f * PI;
            float startDist = 30.0f + (bassIntensity * 50.0f);
            float endDist = startDist + 80.0f + (trebleIntensity * 180.0f);

            float startX = center.x + std::cos(angle) * startDist;
            float startY = center.y + std::sin(angle) * startDist;
            float endX = center.x + std::cos(angle) * endDist + ((float)rand() / RAND_MAX - 0.5f) * 40.0f;
            float endY = center.y + std::sin(angle) * endDist + ((float)rand() / RAND_MAX - 0.5f) * 40.0f;

            std::uint8_t r = rand() % 256;
            std::uint8_t g = rand() % 256;
            std::uint8_t b = rand() % 256;

            sparks[i] = sf::Vertex(sf::Vector2f(startX, startY), sf::Color(r, g, b, 255));
            sparks[i + 1] = sf::Vertex(sf::Vector2f(endX, endY), sf::Color(r, g, b, 0));
        }
        m_primaryBuffer.draw(sparks);
    }

    // --- PHASE 3: SMOOTH ORBITING ENERGY ORBS & THE VOID PORTAL ---
    // Smooth foreground orbs floating calmly over the geometric background chaos
    int numOrbs = 6;
    for (int k = 0; k < numOrbs; ++k) {
        float orbitAngle = (time * 1.2f) + (k * (2.0f * PI / numOrbs));
        int freqSegment = k * (512 / numOrbs);
        float pulseRadius = 80.0f + (m_magnitudes[freqSegment] * 180.0f);

        float ox = center.x + std::cos(orbitAngle) * pulseRadius;
        float oy = center.y + std::sin(orbitAngle) * pulseRadius;

        std::uint8_t r = static_cast<std::uint8_t>(std::sin(orbitAngle + time * 2.0f) * 127.0f + 128.0f);
        std::uint8_t g = static_cast<std::uint8_t>(std::sin(orbitAngle + time * 2.0f + 2.094f) * 127.0f + 128.0f);
        std::uint8_t b = static_cast<std::uint8_t>(std::sin(orbitAngle + time * 2.0f + 4.188f) * 127.0f + 128.0f);

        sf::CircleShape orb(6.0f + bassIntensity * 10.0f);
        orb.setOrigin({orb.getRadius(), orb.getRadius()});
        orb.setPosition({ox, oy});
        orb.setFillColor(sf::Color(r, g, b, 255));
        
        m_primaryBuffer.draw(orb);
    }

    // Soft dark center well to anchoring the visualizer depth
    sf::CircleShape portalCenter(25.0f + (bassIntensity * 15.0f));
    portalCenter.setOrigin({portalCenter.getRadius(), portalCenter.getRadius()});
    portalCenter.setPosition(center);
    portalCenter.setFillColor(sf::Color(0, 0, 0, 80)); 
    m_primaryBuffer.draw(portalCenter);

    m_primaryBuffer.display();

    // Copy to feedback buffer
    m_feedbackBuffer.clear(sf::Color::Black);
    sf::Sprite tempSprite(m_primaryBuffer.getTexture());
    m_feedbackBuffer.draw(tempSprite);
    m_feedbackBuffer.display();
}

void Visualizer::draw(sf::RenderWindow& window) {
    // Draw the final compound texture straight to our display window
    sf::Sprite finalDraw(m_primaryBuffer.getTexture());
    window.draw(finalDraw);
}