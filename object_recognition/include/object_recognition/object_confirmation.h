#ifndef OBJECT_CONFIRMATION_H
#define OBJECT_CONFIRMATION_H

#include <common/parameter.h>
#include <common/types.h>

class ObjectConfirmation
{
public:
    ObjectConfirmation();

    bool update(const common::NameAndProbability& shape, const common::NameAndProbability& color, std::string& confirmed_object);

protected:

    void increment(std::map<std::string, int>& map, const common::NameAndProbability& name);
    double calculate_max_ratio(const std::map<std::string, int>& map, std::string& max_key);
    void reset_accumulation();

    Parameter<double> _min_iterations;
    Parameter<double> _min_ratio;
    Parameter<int> _reset_threshold;

    std::map<std::string, int> _shape_accumulator;
    std::map<std::string, int> _color_accumulator;

    int _empty_frames;
    int _accumulated_frames;
};

#endif // OBJECT_CONFIRMATION_H
