import * as React from 'react';
import ReactDOM from 'react-dom';
import Slider from '@mui/material/Slider';
import axios from 'axios';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Button from '@mui/material/Button';
import Card from '@mui/material/Card';
import CardActions from '@mui/material/CardActions';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import MUIDataTable from 'mui-datatables';

class TempsAndFans extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      readings: null
    };
  }

  componentDidMount() {
    axios.get('../get_temp_control_json/')
        .then((response) => {
          this.setState({
            readings: response.data.data
          }, ()=>{
            console.log(this.state.readings);
          });
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
        name: 'record_time',
        label: 'Timestamp'
      }, {
        name: 'external_temp_0',
        label: 'External0'
      }, {
        name: 'external_temp_1',
        label: 'External1'
      }, {
        name: 'internal_temp_0',
        label: 'Internal0'
      }, {
        name: 'internal_temp_1',
        label: 'Internal1'
      }, {
        name: 'fans_load',
        label: 'Fans Load'
      }
    ];
    const options = {
      download: false,
      print: false,
      search: false,
      pagination: false,
      selectableRowsHeader: false,
      filter: false,
      viewColumns: false,
      selectableRows: 'none'
    };

    if (this.state.readings !== null) {
      return (
        <MUIDataTable title={'Temps and Fans'} data={this.state.readings} columns={columns} options={options} />
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
      doorStates: null
    };
  }

  componentDidMount() {
    axios.get('../get_rack_door_states_json/')
        .then((response) => {
          this.setState({
            doorStates: response.data.data
          }, ()=>{
            console.log(this.state.doorStates);
          });
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
        name: 'record_id',
        label: 'ID',
        options: {
          filter: true,
          sort: true
        }
      },
      {
        name: 'record_time',
        label: 'Timestamp',
        options: {
          filter: true,
          sort: false
        }
      },
      {
        name: 'door_state',
        label: 'IsOpened?',
        options: {
          filter: true,
          sort: false
        }
      }
    ];
    const options = {
      download: false,
      print: false,
      responsive: 'standard',
      selectableRowsHeader: false
    };

    if (this.state.doorStates !== null) {
      return (<MUIDataTable title={'Door State'} data={this.state.doorStates} columns={columns} options={options} />);
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
      imageId: 0
    };
    this.onRangeValueChange = this.onRangeValueChange.bind(this);
  }

  componentDidMount() {
    axios.get('../get_images_list_json/')
        .then((response) => {
          this.setState({
            imagesList: response.data.data,
            imageId: response.data.data.length - 1
          }, ()=>{
            console.log(this.state.imagesList);
          });
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
      imageId: parsedValue
    });
  };

  render() {
    if (this.state.imagesList !== null && typeof this.state.imagesList[this.state.imageId] === 'string') {
      return (
        <Card sx={{maxWidth: 364}}>
          <CardMedia
            component="img"
            height="640"
            image={`../get_images_jpg/?imageName=${this.state.imagesList[this.state.imageId]}`}
            alt={this.state.imagesList[this.state.imageId]}
            sx={{objectFit: 'contain'}}
          />
          <CardContent>
            <Typography gutterBottom variant="h5" component="div">
              CCTV
            </Typography>
          </CardContent>
          <CardActions>
            <Slider
              defaultValue={this.state.imagesList.length - 1} aria-label="Default" valueLabelDisplay="auto"
              min={0} max={this.state.imagesList.length - 1} onChange={this.onRangeValueChange}
              sx={{mx: '2rem'}}
            />
          </CardActions>
        </Card>
      );
    } else {
      return <></>;
    }
  }
}

export default function ButtonAppBar() {
  return (
    <Box sx={{flexGrow: 1, mb: '2rem'}}>
      <AppBar position="static">
        <Toolbar>
          <Typography
            variant="h6"
            noWrap
            component="a"
            href="/"
            sx={{
              mr: 2,
              display: {xs: 'none', md: 'flex'},
              fontFamily: 'monospace',
              fontWeight: 700,
              letterSpacing: '.3rem',
              color: 'inherit',
              textDecoration: 'none'
            }}
          >
            Rack Monitor
          </Typography>
          <Button color="inherit">Login</Button>
        </Toolbar>
      </AppBar>
    </Box>
  );
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date()
    };
  }

  render() {
    return (
      <>
        <ButtonAppBar />
        <div style={{
          maxWidth: '1280px', display: 'block',
          marginLeft: 'auto', marginRight: 'auto'
        }}>
          <Grid container>
            <Grid xs={12} md={4}>
              <Box display="flex" justifyContent="center" alignItems="center">
                <LiveImages />
              </Box>
            </Grid>
            <Grid xs={12} md={8} >
              <DoorState />
              <TempsAndFans />
            </Grid>
          </Grid>
        </div>
      </>
    );
  }
}

const container = document.getElementById('root');
ReactDOM.render(<Index name="Saeloun blog" />, container);
