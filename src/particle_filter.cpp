/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>


using namespace std;

static  default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * Set the number of particles. Initialize all particles to first position  
   * (based on estimates of x, y, theta and their uncertainties from GPS) 
   * and all weights to 1. 
   * Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */

  // Set the number of particles
  num_particles = 10;  

  // Define sensor noise
  normal_distribution<double> n_x(0, std[0]);
  normal_distribution<double> n_y(0, std[1]);
  normal_distribution<double> n_theta(0, std[2]);

  // Initialize particles
  for (int i = 0; i < num_particles; i++) {
    Particle p;
    p.id = i;
    p.x = x + n_x(gen);
    p.y = y + n_y(gen);
    p.theta = theta + n_theta(gen);
    p.weight = 1.0;

    particles.push_back(p);
    weights.push_back(p.weight);
  }

  is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */

  normal_distribution<double> n_x(0, std_pos[0]);
  normal_distribution<double> n_y(0, std_pos[1]);
  normal_distribution<double> n_theta(0, std_pos[2]);

  for (int i = 0; i < num_particles; i++) {
    if (abs(yaw_rate) == 0) {
      particles[i].x += velocity * delta_t * cos(particles[i].theta);
      particles[i].y += velocity * delta_t * sin(particles[i].theta);
    }
    else {
      particles[i].x += velocity / yaw_rate * (sin(particles[i].theta \
            + yaw_rate*delta_t) - sin(particles[i].theta));
      particles[i].y += velocity / yaw_rate * (cos(particles[i].theta)\
            - cos(particles[i].theta + yaw_rate*delta_t));
      particles[i].theta += yaw_rate * delta_t;
    }

    // Add noise
    particles[i].x += n_x(gen);
    particles[i].y += n_y(gen);
    particles[i].theta += n_theta(gen);
  }

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */

  for (int i = 0; i < observations.size(); i++) {
    
    // Get current observation
    LandmarkObs obs = observations[i];

    // Init min distance
    double min_dist = numeric_limits<double>::max();

    // Init map id
    int map_id = -1;

    for (int j = 0; j < predicted.size(); j++) {
      
      // Get current prediction
      LandmarkObs pred = predicted[j];

      // Cal distance
      double cur_dist = dist(obs.x, obs.y, pred.x, pred.y);

      // Find the nearest prediction towards current observation
      if (cur_dist < min_dist) {
        min_dist = cur_dist;
        map_id = pred.id;
      }
    }

    // Set observation's id to the nearest predicted landmark's id
    observations[i].id = map_id;
    
  }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  double sum_weights = 0.0;

  // Loop through each particle to update weights
  for (int i = 0; i < num_particles; i++) {
    
    double p_x = particles[i].x;
    double p_y = particles[i].y;
    double p_theta = particles[i].theta;

    // Get possible predictions with the sensor range
    vector<LandmarkObs> sensor_range_pred;

    for (int j = 0; j < map_landmarks.landmark_list.size(); j++) {
      float lm_x  = map_landmarks.landmark_list[j].x_f;
      float lm_y  = map_landmarks.landmark_list[j].y_f;
      int   lm_id = map_landmarks.landmark_list[j].id_i;
      
      // Calculate the distance
      double d = dist(lm_x, lm_y, p_x, p_y);
      if (d <= sensor_range) {
        sensor_range_pred.push_back(LandmarkObs{lm_id, lm_x, lm_y});
      }
    }

    // Translate observations into map coordinate
    vector<LandmarkObs> trans_obs;
    for (int k = 0; k < observations.size(); k++) {
      double obs_x = cos(p_theta) * observations[k].x - sin(p_theta) * \
                     observations[k].y + p_x;
      double obs_y = sin(p_theta) * observations[k].x + cos(p_theta) * \
                     observations[k].y + p_y;
      trans_obs.push_back(LandmarkObs{observations[k].id, obs_x, obs_y});
    }

    // Data association for map landmarks with sensor range and transformed
    // observation
    dataAssociation(sensor_range_pred, trans_obs);

    // Intialize weight
    particles[i].weight = 1.0;

    double sig_x = std_landmark[0];
    double sig_y = std_landmark[1];
    double multiplier = 1/(2 * M_PI * sig_x * sig_y);
    double sig_x_sq = pow(sig_x, 2);
    double sig_y_sq = pow(sig_y, 2);    

    for (int j = 0; j < trans_obs.size(); j++) {
      
      double obs_x = trans_obs[j].x;
      double obs_y = trans_obs[j].y;
      int pred_lm_id = trans_obs[j].id;
      double pred_x, pred_y;

      // Get x,y of the prediction associated with current observation
      for (int k = 0; k < sensor_range_pred.size(); k++) {
        if (sensor_range_pred[k].id == pred_lm_id) {
          pred_x = sensor_range_pred[k].x;
          pred_y = sensor_range_pred[k].y;
        }
      }


      // Calculate the weight for this observation
      double w =  multiplier * exp(-( pow(pred_x-obs_x,2)\
           /(2 * sig_x_sq) + (pow(pred_y-obs_y, 2)/(2 * sig_y_sq))));
      
      // Update the particle weight
      particles[i].weight *= w;
    }

    sum_weights += particles[i].weight;
  }

  // Nornalize weights for all particles
  for (int i = 0; i < particles.size(); i++) {
    particles[i].weight /= sum_weights;
    weights[i] = particles[i].weight;
  }

}

void ParticleFilter::resample() {
  /**
   * Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

  vector<Particle> new_particles;

  // Generate random starting index
  uniform_int_distribution<int> u_int_dist(0, num_particles - 1);
  int index = u_int_dist(gen);

  // Get max weight
  double max_weight = 2 * *max_element(weights.begin(), weights.end());

  // Uniform distribution in [0.0, max_weight]
  uniform_real_distribution<double> u_real_dist(0.0, max_weight);

  // Resample
  double beta = 0.0;
  for (int i = 0; i < num_particles; i++) {
    beta += u_real_dist(gen);
    while (beta > weights[index]) {
      beta -= weights[index];
      index = (index + 1) % num_particles;
    }
    new_particles.push_back(particles[index]);
  }
  particles = new_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}
