#include <string.h>
#include <libasync/generator.h>

namespace libasync
{   //Generator error internal constructor
    GeneratorError::GeneratorError(GeneratorError::Reason __reason, const char* _error_info)
        : _reason(__reason), error_info(_error_info) {}

    //Get reason
    GeneratorError::Reason GeneratorError::reason() const noexcept
    {   return this->_reason;
    }

    //Get error information
    const char* GeneratorError::what() const noexcept
    {   return this->error_info;
    }
}
