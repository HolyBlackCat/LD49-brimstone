#pragma once

struct Sky
{
    unsigned int time = 0;

    Sky() {}

    void Move();
    void Render() const;
};
