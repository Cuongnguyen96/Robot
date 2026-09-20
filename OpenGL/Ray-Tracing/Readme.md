# Lagrange Multipliers

**Lagrange Multipliers** is a powerful mathematical optimization technique used to **find the local maxima or minima of a function subject to one or more constraints**. 

Instead of trying to substitute variables to eliminate the constraint, this method transforms a constrained optimization problem into an unconstrained one by introducing auxiliary variables called **Lagrange multipliers (λ)**.

---

## 1. Geometric Intuition
Suppose you want to optimize an objective function $f(x, y)$ subject to a constraint curve $g(x, y) = c$. 

The extrema will occur at the exact point where the level curve of $f(x, y)$ is **tangent** to the constraint curve $g(x, y) = c$. When two curves are tangent at a point, their normal vectors (gradient vectors) at that point will be **collinear** (parallel) to each other.

Mathematics establishes this relationship with the formula:
$$\nabla f = \lambda \nabla g$$
In which $\lambda$ (Lambda) is a real number, which is the **Lagrange multiplier**.

---

## 2. Step-by-Step Guide to Solving Problems
To find the extrema of a function $f(x, y, z)$ subject to the constraint condition $g(x, y, z) = c$, perform the following steps:

* **Step 1: Set up the Lagrange function (Lagrangian):**
  $$L(x, y, z, \lambda) = f(x, y, z) - \lambda(g(x, y, z) - c)$$

* **Step 2: Find the partial derivatives** of $L$ with respect to each variable and set them to zero to form a system of equations:
  $$\begin{cases} \nabla f = \lambda \nabla g \\ g(x, y, z) = c \end{cases} \implies \begin{cases} f_x = \lambda g_x \\ f_y = \lambda g_y \\ f_z = \lambda g_z \\ g(x, y, z) = c \end{cases}$$

* **Step 3: Solve the system of equations** above to find the critical points $(x_0, y_0, z_0)$. (The value of $\lambda$ is just a helper tool and is not necessarily used in the final step).

* **Step 4: Calculate the value of the objective function $f$** at the critical points found. The largest value will be the maximum, and the smallest value will be the minimum.

---

## 3. Concrete Application Example
**Problem:** Find the extrema of the function $f(x, y) = x^2 + y^2$ subject to the constraint $x + y = 2$.

* **Identify the constraint function:** $g(x, y) = x + y = 2$.
* **Calculate the Gradients:** $\nabla f = (2x, 2y)$ and $\nabla g = (1, 1)$.
* **Set up the system of equations ($\nabla f = \lambda \nabla g$ and $g=2$):**
  $$\begin{cases} 2x = \lambda \cdot 1 \\ 2y = \lambda \cdot 1 \\ x + y = 2 \end{cases}$$
* **Solve the system:** From the first two equations, we have $2x = 2y \implies x = y$. Substituting this into the third equation: $x + x = 2 \implies x = 1 \implies y = 1$.
* **Conclusion:** The point to find is $(1, 1)$. The minimum value of the function under this constraint is $f(1, 1) = 1^2 + 1^2 = 2$.

---

## 4. Real-World Applications
This method is the core foundation in many fields:
* **Economics:** Used to maximize consumer utility based on budget limits, or minimize production costs for businesses. In this context, $\lambda$ is called the **shadow price**, showing the marginal value gained when relaxing the constraint by 1 unit.
* **Machine Learning:** Directly applied in the [Support Vector Machines (SVM)](https://wikipedia.org) algorithm to find the optimal separating hyperplane between data classes.
* **Physics & Engineering:** Helps optimize material structures under boundary pressures or calculate analytical mechanics (Euler-Lagrange equations).