/* -*- compile-command: "R CMD INSTALL .." -*- */

#include <vector>
#include <stdio.h>
#include "funPieceList.h"
#include <math.h>

void PeakSegPDPA
(double *data_vec, double *weight_vec, int data_count,
 int maxSegments,
 // the following matrices are for output:
 double *cost_mat, //data_count x maxSegments.
 int *end_mat,//maxSegments x maxSegments(model up to last data point).
 double *mean_mat,
 int *intervals_mat){//maxSegments x maxSegments(model up to last data point).
  double min_mean=data_vec[0], max_mean=data_vec[0];
  for(int data_i=1; data_i<data_count; data_i++){
    if(data_vec[data_i] < min_mean){
      min_mean = data_vec[data_i];
    }
    if(max_mean < data_vec[data_i]){
      max_mean = data_vec[data_i];
    }
  }
  std::vector<PiecewisePoissonLoss> cost_model_vec(data_count * maxSegments);
  double Log_cumsum = 0;
  double Linear_cumsum = 0;
  for(int data_i=0; data_i<data_count; data_i++){
    Linear_cumsum += weight_vec[data_i];
    Log_cumsum += -data_vec[data_i]*weight_vec[data_i];
    PiecewisePoissonLoss *cost_model = &cost_model_vec[data_i];
    cost_model->piece_list.emplace_back
      (Linear_cumsum, Log_cumsum, 0.0, min_mean, max_mean, -1, false);
  }

  // DP: compute functional model of best cost in S segments up to
  // data point N.
  PiecewisePoissonLoss *prev_cost_model, *new_cost_model;
  PiecewisePoissonLoss min_prev_cost, cost_model;
  for(int total_changes=1; total_changes<maxSegments; total_changes++){
    for(int data_i=total_changes; data_i<data_count; data_i++){
      int prev_i = data_i-1;
      prev_cost_model = &cost_model_vec[prev_i + (total_changes-1)*data_count];
      //printf("DP changes=%d data_i=%d\n", total_changes, data_i);
      //printf("prev cost model\n");
      //prev_cost_model->print();
      if(total_changes % 2){
	min_prev_cost.set_to_min_less_of(prev_cost_model);
      }else{
	min_prev_cost.set_to_min_more_of(prev_cost_model);
      }
      min_prev_cost.set_prev_seg_end(prev_i);
      new_cost_model = &cost_model_vec[data_i + total_changes*data_count];
      if(data_i==total_changes){//first cost model, only one candidate.
	//printf("new cost model = min prev cost\n");
	//min_prev_cost.print();
	*new_cost_model = min_prev_cost;
      }else{
	//printf("min prev cost\n");
	//min_prev_cost.print();
	//printf("cost model\n");
	//cost_model.print();
	new_cost_model->set_to_min_env_of(&min_prev_cost, &cost_model);
      }
      //printf("new cost model\n");
      //new_cost_model->print();
      new_cost_model->add
	(weight_vec[data_i],
	 -data_vec[data_i]*weight_vec[data_i],
	 0.0);
      //new_cost_model->print();
      cost_model = *new_cost_model;
    }
  }

  double best_cost, best_mean;
  double *best_mean_vec;
  int *prev_seg_vec;
  bool equality_constraint_active;
  int prev_seg_end;
  
  // Decoding the cost_model_vec, and writing to the output matrices.
  for(int i=0; i<maxSegments*maxSegments; i++){
    mean_mat[i] = INFINITY;
    end_mat[i] = -1;
  }
  for(int i=0; i<maxSegments*data_count; i++){
    intervals_mat[i] = -1;
    cost_mat[i] = INFINITY;
  }
  for(int total_changes=0; total_changes<maxSegments;total_changes++){
    for(int data_i=total_changes; data_i<data_count; data_i++){
      //printf("decoding changes=%d data_i=%d\n", total_changes, data_i);
      PiecewisePoissonLoss *cost_model =
	&cost_model_vec[data_i + total_changes*data_count];
      cost_model->Minimize
	(&best_cost, &best_mean,
	 &prev_seg_end, &equality_constraint_active);
      // for the models up to any data point, we store the best cost
      // and the total number of intervals.
      cost_mat[data_i + total_changes*data_count] = best_cost;
      intervals_mat[data_i + total_changes*data_count] =
	cost_model->piece_list.size();
      if(data_i == data_count-1){
	// for the models up to the last data point, we also store the
	// segment means and the change-points. we have already
	// computed the values for the last segment so store them.
	best_mean_vec = mean_mat + total_changes*maxSegments;
	prev_seg_vec = end_mat + total_changes*maxSegments;
	best_mean_vec[total_changes] = best_mean;
	prev_seg_vec[total_changes] = prev_seg_end;
	for(int seg_i=total_changes-1; 0 <= seg_i; seg_i--){
	  //printf("seg_i=%d prev_seg_end=%d\n", seg_i, prev_seg_end);
	  cost_model = &cost_model_vec[prev_seg_end + seg_i*data_count];
	  if(equality_constraint_active){
	    cost_model->findMean
	      (best_mean, &prev_seg_end, &equality_constraint_active);
	  }else{
	    cost_model->Minimize(&best_cost, &best_mean,
				&prev_seg_end, &equality_constraint_active);
	  }
	  best_mean_vec[seg_i] = best_mean;
	  prev_seg_vec[seg_i] = prev_seg_end;
	}//for(seg_i
      }//if(data_i
    }//for(data_i
  }//for(total_changes
}
