#ifndef PARTICLE_SYSTEM_HPP_INCLUDED
#define PARTICLE_SYSTEM_HPP_INCLUDED

#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include "formula_callable.hpp"
#include "geometry.hpp"
#include "wml_node_fwd.hpp"

class entity;
class particle_system;
typedef boost::intrusive_ptr<particle_system> particle_system_ptr;
typedef boost::intrusive_ptr<const particle_system> const_particle_system_ptr;

class particle_system_factory;
typedef boost::shared_ptr<const particle_system_factory> const_particle_system_factory_ptr;

class particle_system_factory
{
public:
	static const_particle_system_factory_ptr create_factory(wml::const_node_ptr node);
	
	virtual ~particle_system_factory();
	virtual particle_system_ptr create(const entity& e) const = 0;
};

class particle_system : public game_logic::formula_callable
{
public:
	virtual ~particle_system();
	virtual bool is_destroyed() const { return false; }
	virtual bool should_save() const { return true; }
	virtual void process(const entity& e) = 0;
	virtual void draw(const rect& area, const entity& e) const = 0;
private:
};

#endif
