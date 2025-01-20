#pragma once
#include <numbers>
#include <cmath>
#include <Windows.h>

struct view_matrix_t
{
    float* operator[](int index)
    {
        return matrix[index];
    }

    float matrix[4][4];
};

class Vector
{
public:
    constexpr Vector(
        const float x = 0.f,
        const float y = 0.f,
        const float z = 0.f) noexcept :
        x(x), y(y), z(z) {
    }

    bool operator==(const Vector& other) const noexcept;

    Vector operator-(const Vector& other) const noexcept; // Changed return type
    Vector operator+(const Vector& other) const noexcept; // Changed return type
    Vector operator/(const float factor) const noexcept; // Changed return type
    Vector operator*(const float factor) const noexcept; // Changed return type

    static bool world_to_screen(view_matrix_t view_matrix, Vector& in, Vector& out);

    bool IsZero() const; // Added const

    float DistanceTo(const Vector& other) const;

    float length() const;

    float x, y, z;
};