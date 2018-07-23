/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
	default_random_engine gen;
	num_particles = 10;
	// Create normal (Gaussian) distributions for x, y and theta.
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(x, std[1]);
	normal_distribution<double> dist_theta(x, std[2]);
	// initialize each particle
	for (int i = 0; i < num_particles; i++) {
		Particle new_particle;
		double sample_x = dist_x(gen); 
		double sample_y = dist_y(gen); 
		double sample_theta = dist_theta(gen); 
		new_particle.id = i;
		new_particle.x = sample_x;
		new_particle.y = sample_y;
		new_particle.theta = sample_theta;
		new_particle.weight = 1.0;	
		weights.push_back(new_particle.weight);
		particles.push_back(new_particle);
	}
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/
	default_random_engine gen;
	for (int i = 0; i < num_particles; i++) {
		double yaw_dt = yaw_rate * delta_t;
		double new_x = particles[i].x + (velocity/yaw_rate) * ( sin(particles[i].theta + yaw_dt) - sin(particles[i].theta) );
		double new_y = particles[i].y + (velocity/yaw_rate) * ( cos(particles[i].theta) - cos(particles[i].theta + yaw_dt) );
		double new_theta = particles[i].theta + yaw_dt;
		// Create normal (Gaussian) distributions for x, y and theta.
		normal_distribution<double> dist_x(new_x, std_pos[0]);
		normal_distribution<double> dist_y(new_y, std_pos[1]);
		normal_distribution<double> dist_theta(new_theta, std_pos[2]);
		// update the parameters of each particle
		particles[i].x = dist_x(gen);
		particles[i].y = dist_y(gen);
		particles[i].theta = dist_theta(gen); 
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.
	for (int i = 0; i < observations.size(); i++) {
		double min_dist = 999999999.9;
		for (int j = 0; i < predicted.size(); j++) {
			double distance = dist(observations[i].x, observations[i].y, predicted[j].x, predicted[j].y);	
			if (min_dist < distance) {
				min_dist = distance;
				// observations[i].id = predicted[j].id;
				observations[i].id = j;
			}
		}	
		// TODO: may need to check if the predicted landmark has already been used for another observation
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	// default_random_engine gen;	
	// iterate through each particle
	for (int i = 0; i < num_particles; i++) {
		Particle particle = particles[i];
		// generate a list of predicted measurements, it contains all the map landmarks within the sensor range.
		std::vector<LandmarkObs> predicted;
		for (int j = 0; j < map_landmarks.landmark_list.size(); j++) {
			int landmark_id = map_landmarks.landmark_list[j].id_i;
			float landmark_x = map_landmarks.landmark_list[j].x_f;
			float landmark_y = map_landmarks.landmark_list[j].y_f;
			double distance = dist(particle.x, particle.y, landmark_x, landmark_y);
			if (distance <= sensor_range) {
				LandmarkObs new_landmark;
				new_landmark.x = landmark_x;
				new_landmark.y = landmark_y;
				new_landmark.id = landmark_id; 
				predicted.push_back(new_landmark);
			} 
		}
		// convert the observations from vehicle coordinates to map coordinates
		std::vector<LandmarkObs> obs_in_map_coordinates;
		for (int j = 0; j < observations.size(); j++) {
			LandmarkObs current_obs = observations[j];
			LandmarkObs converted_obs;
			converted_obs.x = particle.x + (cos(particle.theta)*current_obs.x) - (sin(particle.theta)*current_obs.y);
			converted_obs.y = particle.y + (sin(particle.theta)*current_obs.x) + (cos(particle.theta)*current_obs.y);
			obs_in_map_coordinates.push_back(converted_obs);
		}
		// find the associations between predicted measurements and observed measurements
		dataAssociation(predicted, obs_in_map_coordinates);

		// compute the weight of this particle
		double weight = 1.0;
		double std_x = std_landmark[0];
		double std_y = std_landmark[1];
		for (int i = 0; i < obs_in_map_coordinates.size(); i++) {
			// assume that we use the index of predicted measurements list as id, not the id of actual landmark.
			int meas_idx = obs_in_map_coordinates[i].id;	
			double landmark_x = predicted[meas_idx].x;
			double landmark_y = predicted[meas_idx].y;
			double meas_x = obs_in_map_coordinates[i].x;
			double meas_y = obs_in_map_coordinates[i].y;
			double a = pow(meas_x - landmark_x, 2.0) / (2 * pow(std_x, 2.0));
			double b = pow(meas_y - landmark_y, 2.0) / (2 * pow(std_y, 2.0));
			double cur_weight = 1/(2*M_PI*std_x*std_y) * exp(-(a+b));
			weight *= cur_weight;
		}
		particle.weight = weight;
		weights[i] = weight;
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
	default_random_engine gen;
	discrete_distribution<int> dist(weights.begin(), weights.end());
	std::vector<Particle> new_particles;
	for (int i = 0; i < num_particles; i++) {
		int new_idx = dist(gen);
		new_particles.push_back(particles[new_idx]);
	}
	particles = new_particles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
