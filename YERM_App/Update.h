#ifndef __UPDATE_H__
#define __UPDATE_H__

#include <chrono>
#include <vector>

namespace onart
{

    class Updator{
        public:
            static void update(std::chrono::nanoseconds dt);
            static void finalize();
    };

    class Updatee{
        friend class Updator;
        public:
            virtual ~Updatee();
        protected:
            Updatee(std::chrono::nanoseconds period = {}, uint32_t limit = 0, int priority = 0);
        private:
            bool run(std::chrono::nanoseconds dt);
            virtual void update(std::chrono::nanoseconds dt) = 0;

        private:
            const std::chrono::nanoseconds period;
            std::chrono::nanoseconds clock;
            const uint32_t limit;
            uint32_t limitCounter;
            Updatee** internal;
            int priority;
    };
} // namespace  onart


#endif