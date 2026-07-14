#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>

class Visualizer {
public:
    Visualizer(unsigned int width, unsigned int height);
    
    void update(const sf::Sound& sound, const sf::SoundBuffer& buffer);
    void draw(sf::RenderWindow& window);

private:
    unsigned int m_width;
    unsigned int m_height;
    
    // FFT and color tracking
    sf::Clock m_colorClock;
    std::vector<float> m_magnitudes; 

    // Milkdrop-style feedback system buffers (Order matters for C++ initialization!)
    sf::RenderTexture m_primaryBuffer;
    sf::RenderTexture m_feedbackBuffer;
    sf::Sprite m_feedbackSprite;
    
    // Graphic shapes driven by FFT
    float m_rotationAngle;
    sf::VertexArray m_innerRing;
};

#endif // VISUALIZER_H