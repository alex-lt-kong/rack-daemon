import * as React from "react";
import ReactDOM from "react-dom";
import Slider from "@mui/material/Slider";
import axios from "axios";
import Grid from "@mui/material/Grid"; // Grid version 1
import AppBar from "@mui/material/AppBar";
import Box from "@mui/material/Box";
import Toolbar from "@mui/material/Toolbar";
import Typography from "@mui/material/Typography";
import Card from "@mui/material/Card";
import Button from "@mui/material/Button";
import CardActions from "@mui/material/CardActions";
import CardContent from "@mui/material/CardContent";
import CardMedia from "@mui/material/CardMedia";
import MUIDataTable from "mui-datatables";
import StorageIcon from "@mui/icons-material/Storage";

class TempsAndFans extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      readings: null,
    };
  }

  componentDidMount() {
    axios
      .get("../get_temp_control_json/")
      .then((response) => {
        this.setState(
          {
            readings: [response.data],
          },
          () => {
            console.log(this.state.readings);
          }
        );
      })
      .catch((error) => {
        console.log(error);
        alert(`${error}`);
        // You canNOT write error.response or whatever similar here.
        // The reason is that this catch() catches both network error and other errors,
        // which may or may not have a response property.
      });
  }

  render() {
    function getTempString(value) {
      console.log(value);
      const BAD_TEMP = 65535;
      const rawReadings = value.split(",").map((ele) => Number(ele));
      let readings = "";
      for (let i = 0; i < rawReadings.length; ++i) {
        if (rawReadings[i] != BAD_TEMP) {
          readings += `${Math.round((rawReadings[i] / 1000) * 10) / 10}--°C`;
        } else {
          readings += `<Offline>`;
        }
        if (i < rawReadings.length - 1) {
          readings += ", ";
        }
      }
      return readings;
    }
    const columns = [
      {
        name: "record_time",
        label: "Timestamp",
      },
      {
        name: "external_temps",
        label: "External sensors reading",
        options: {
          customBodyRender: (value, tableMeta, updateValue) =>
            getTempString(value),
        },
      },
      {
        name: "internal_temps",
        label: "Internal sensors reading",
        options: {
          customBodyRender: (value, tableMeta, updateValue) =>
            getTempString(value),
        },
      },
      {
        name: "fans_load",
        label: "Fans Load",
        options: {
          customBodyRender: (value, tableMeta, updateValue) => `${value}%`,
        },
      },
    ];
    const options = {
      download: false,
      print: false,
      search: false,
      pagination: false,
      selectableRowsHeader: false,
      filter: false,
      viewColumns: false,
      selectableRows: "none",
    };

    if (this.state.readings !== null) {
      return (
        <MUIDataTable
          title={"Temps and Fans"}
          data={this.state.readings}
          columns={columns}
          options={options}
        />
      );
    } else {
      return <></>;
    }
  }
}

class DoorState extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      doorStates: null,
    };
  }

  componentDidMount() {
    axios
      .get("../get_rack_door_states_json/")
      .then((response) => {
        this.setState(
          {
            doorStates: response.data.data,
          },
          () => {
            console.log(this.state.doorStates);
          }
        );
      })
      .catch((error) => {
        console.log(error);
        alert(`${error}`);
        // You canNOT write error.response or whatever similar here.
        // The reason is that this catch() catches both network error and other errors,
        // which may or may not have a response property.
      });
  }

  render() {
    const columns = [
      {
        name: "record_id",
        label: "ID",
      },
      {
        name: "record_time",
        label: "Timestamp",
      },
      {
        name: "door_state",
        label: "State",
        options: {
          customBodyRender: (value, tableMeta, updateValue) => {
            if (value === 0) {
              return (
                <Typography fontWeight={700} color="primary">
                  Closed
                </Typography>
              );
            } else {
              return (
                <Typography fontWeight={700} color="error">
                  Opened
                </Typography>
              );
            }
          },
        },
      },
    ];
    const options = {
      download: false,
      print: false,
      responsive: "standard",
      selectableRowsHeader: false,
      sortOrder: {
        name: "record_time",
        direction: "asc",
      },
    };

    if (this.state.doorStates !== null) {
      return (
        <MUIDataTable
          title={"Door State"}
          data={this.state.doorStates}
          columns={columns}
          options={options}
        />
      );
    } else {
      return <></>;
    }
  }
}

class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      imagesList: [],
      imageId: 0,
    };
    this.onRangeValueChange = this.onRangeValueChange.bind(this);
    this.currentlyShowing = this.currentlyShowing.bind(this);
  }

  componentDidMount() {
    axios
      .get("../get_images_list_json/")
      .then((response) => {
        this.setState(
          {
            imagesList: response.data.data,
            imageId: response.data.data.length - 1,
          },
          () => {
            console.log(this.state.imagesList);
          }
        );
      })
      .catch((error) => {
        console.log(error);
        alert(`${error}`);
        // You canNOT write error.response or whatever similar here.
        // The reason is that this catch() catches both network error and other errors,
        // which may or may not have a response property.
      });
  }

  onRangeValueChange(event) {
    const parsedValue = parseInt(event.target.value);
    this.setState({
      imageId: parsedValue,
    });
  }

  currentlyShowing() {
    return this.state.imagesList[this.state.imageId];
  }

  render() {
    if (
      this.state.imagesList !== null &&
      typeof this.state.imagesList[this.state.imageId] === "string"
    ) {
      return (
        <Card sx={{ maxWidth: 364 }}>
          <CardMedia
            component="img"
            height="640"
            image={`../get_images_jpg/?imageName=${
              this.state.imagesList[this.state.imageId]
            }`}
            alt={this.state.imagesList[this.state.imageId]}
            sx={{ objectFit: "contain" }}
          />
          <CardContent>
            <Typography gutterBottom variant="h5" component="div">
              CCTV
            </Typography>
          </CardContent>
          <CardActions>
            <Slider
              defaultValue={this.state.imagesList.length - 1}
              aria-label="Default"
              valueLabelDisplay="auto"
              valueLabelFormat={this.currentlyShowing}
              min={0}
              max={this.state.imagesList.length - 1}
              onChange={this.onRangeValueChange}
              sx={{ mx: "2rem" }}
            />
          </CardActions>
        </Card>
      );
    } else {
      return <></>;
    }
  }
}

class NavBar extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      user: null,
    };
  }

  componentDidMount() {
    axios
      .get("../get_logged_in_user/")
      .then((response) => {
        this.setState({
          user: response.data.data,
        });
      })
      .catch((error) => {
        console.log(error);
        alert(`${error}`);
      });
  }
  render() {
    return (
      <Box sx={{ flexGrow: 1, mb: "1rem" }}>
        <AppBar position="static">
          <Toolbar>
            <StorageIcon sx={{ display: { md: "flex" }, mr: 1 }} />
            <Typography
              variant="h6"
              noWrap
              component="a"
              href="/"
              sx={{
                mr: 2,
                display: { md: "flex" },
                fontFamily: "monospace",
                fontWeight: 700,
                letterSpacing: ".1rem",
                color: "inherit",
                textDecoration: "none",
              }}
            >
              RACK MONITOR
            </Typography>
            <Typography
              variant="h6"
              component="div"
              sx={{ flexGrow: 1 }}
            ></Typography>
            <Button color="inherit">{this.state.user}</Button>
          </Toolbar>
        </AppBar>
      </Box>
    );
  }
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date(),
    };
  }

  render() {
    return (
      <>
        <NavBar />
        <div
          style={{
            maxWidth: "1280px",
            display: "block",
            marginLeft: "auto",
            marginRight: "auto",
          }}
        >
          <Grid container>
            <Grid xs={12} md={4}>
              <Box
                display="flex"
                justifyContent="center"
                alignItems="center"
                mb="2rem"
              >
                <LiveImages />
              </Box>
            </Grid>
            <Grid xs={12} md={8}>
              <Box mb="2rem">
                <DoorState />
              </Box>
              <TempsAndFans />
            </Grid>
          </Grid>
        </div>
      </>
    );
  }
}

const container = document.getElementById("root");
ReactDOM.render(<Index />, container);
