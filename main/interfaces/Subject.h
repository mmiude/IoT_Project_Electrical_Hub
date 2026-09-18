#ifndef SUBJECT_H
#define SUBJECT_H

#include <memory>
#include "Observer.h"

class Subject {
public:
    virtual ~Subject() = default; 
    virtual void attach(std::shared_ptr<Observer> obs) = 0;
    virtual void notify(int state) = 0; 
 };

#endif //SUBJECT_H