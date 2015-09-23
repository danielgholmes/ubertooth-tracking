#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

#define N 3

struct data {
    size_t n;
    double *_x;
    double *_y;
    double *_r;
};

int circle_f(const gsl_vector *x, void *data, gsl_vector *f)
{
    size_t n = ((struct data *)data)->n;
    double *_x = ((struct data *)data)->_x;
    double *_y = ((struct data *) data)->_y;
    double *_r = ((struct data *) data)->_r; 

    double x_e = gsl_vector_get(x, 0);
    double y_e = gsl_vector_get(x, 1);

    size_t i;

    for (i = 0; i < n; i++)
    {
        double t = i;
        double y_i = _r[i] - sqrt(pow(_x[i] - x_e, 2.0) + pow(_y[i] - y_e, 2.0));
        gsl_vector_set(f, i, y_i);
    }

    return GSL_SUCCESS;
}

int circle_df(const gsl_vector *x, void *data, gsl_matrix *J)
{
    size_t n = ((struct data *)data)->n;
    double *_x = ((struct data *) data)->_x;
    double *_y = ((struct data *) data)->_y;

    double x_e = gsl_vector_get(x, 0);
    double y_e = gsl_vector_get(x, 1);

    size_t i;

    for (i = 0; i < n; i++)
    {
        double n1 = _x[i] - x_e;
        double n2 = _y[i] - y_e;
        double denom = sqrt(pow(_x[i] - x_e, 2.0) + pow(_y[i] - y_e, 2.0));
        gsl_matrix_set (J, i, 0, n1/denom); 
        gsl_matrix_set (J, i, 1, n2/denom);
    }
    return GSL_SUCCESS;
}

int circle_fdf(const gsl_vector *x, void *data, gsl_vector *f, gsl_matrix *J)
{
    circle_f(x, data, f);
    circle_df(x, data, J);

    return GSL_SUCCESS;
}

void print_state (size_t iter, gsl_multifit_fdfsolver * s)
{
    printf ("iter: %3u x = % 15.8f % 15.8f % 15.8f "
            "|f(x)| = %g\n",
            iter,
            gsl_vector_get(s->x, 0), 
            gsl_vector_get(s->x, 1),
            gsl_blas_dnrm2(s->f));
}

int main (void)
{
    const gsl_multifit_fdfsolver_type *T;
    gsl_multifit_fdfsolver *s;
    int status;
    unsigned int i, iter = 0;
    const size_t n = N;
    const size_t p = 2;

    double _x[N] = {0.0, 5.0, 2.5};
    double _y[N] = {0.0, 0.0, 4.330127019};
    double _r[N] = {2.0, 2.5, 3.0};

    struct data d = {n, _x, _y, _r};
    gsl_multifit_function_fdf f;
    double x_init[2] = {2.2750, 0.9959}; // initial values
    gsl_vector_view x = gsl_vector_view_array (x_init, p);

    f.f = &circle_f;
    f.df = &circle_df;
    f.fdf = &circle_fdf;
    f.n = n;
    f.p = p;
    f.params = &d;

    T = gsl_multifit_fdfsolver_lmsder;
    s = gsl_multifit_fdfsolver_alloc(T, n, p);
    gsl_multifit_fdfsolver_set(s, &f, &x.vector);

    print_state(iter, s);

    do
    {
        iter++;
        status = gsl_multifit_fdfsolver_iterate(s);

        printf ("status = %s\n", gsl_strerror(status));

        print_state(iter, s);

        if (status)
        break;

        status = gsl_multifit_test_delta(s->dx, s->x, 1e-4, 1e-4);
    }
    while (status == GSL_CONTINUE && iter < 500);

    gsl_multifit_fdfsolver_free(s);
    return 0;
}


