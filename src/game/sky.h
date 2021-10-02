#pragma once

class Sky
{
    unsigned int time = 0;

  public:
    Sky() {}

    void Move();
    void Render() const;
};
